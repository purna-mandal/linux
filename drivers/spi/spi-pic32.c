/*
 * PIC32 SPI core controller driver (refer dw_spi.c)
 *
 * Copyright (c) 2014, Microchip Technology Inc.
 *      Purna Chandra Mandal <purna.mandal@microchip.com>
 *
 * Licensed under GPLv2.
 */

#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>

/* SPI Register offsets */
#define SPIxCON		0x00
#define SPIxCON_CLR	0x04
#define SPIxCON_SET	0x08
#define SPIxSTAT	0x10
#define SPIxSTAT_CLR	0x14
#define SPIxBUF		0x20
#define SPIxBRG		0x30
#define SPIxCON2	0x40
#define SPIxCON2_CLR	0x44
#define SPIxCON2_SET	0x48

/* Bit fields in SPIxCON Register */
#define SPIxCON_RXI_SHIFT	0  /* Rx interrupt generation condition */
#define SPIxCON_TXI_SHIFT	2  /* TX interrupt generation condition */
#define SPIxCON_MSTEN		(1 << 5) /* Enable SPI Master */
#define SPIxCON_CKP		(1 << 6) /* active low */
#define SPIxCON_CKE		(1 << 8) /* Tx on falling edge */
#define SPIxCON_SMP		(1 << 9) /* Rx at middle or end of tx */
#define SPIxCON_BPW		0x03	 /* Bits per word/audio-sample */
#define SPIxCON_BPW_SHIFT	10
#define SPIxCON_SIDL		(1 << 13) /* STOP on idle */
#define SPIxCON_ON		(1 << 15) /* Macro enable */
#define SPIxCON_ENHBUF		(1 << 16) /* Enable enhanced buffering */
#define SPIxCON_MCLKSEL		(1 << 23) /* Select SPI Clock src */
#define SPIxCON_MSSEN		(1 << 28) /* SPI macro will drive SS */
#define SPIxCON_FRMEN		(1 << 31) /* Enable framing mode */

/* Bit fields in SPIxSTAT Register */
#define STAT_RF_FULL		(1 << 0) /* RX fifo full */
#define STAT_TF_FULL		(1 << 1) /* TX fifo full */
#define STAT_TX_EMPTY		(1 << 3) /* standard buffer mode */
#define STAT_RF_EMPTY		(1 << 5) /* RX Fifo empty */
#define STAT_RX_OV		(1 << 6) /* err, s/w needs to clear */
#define STAT_SHIFT_REG_EMPTY	(1 << 7) /* Internal shift-reg empty */
#define STAT_TX_UR		(1 << 8) /* UR in Framed SPI modes */
#define STAT_BUSY		(1 << 11) /* Macro is processing tx or rx */
#define STAT_FRM_ERR		(1 << 12) /* Multiple Frame Sync pulse */
#define STAT_TF_LVL_MASK	0x1F
#define STAT_TF_LVL_SHIFT	16
#define STAT_RF_LVL_MASK	0x1F
#define STAT_RF_LVL_SHIFT	24

/* Bit fields in SPIxBRG Register */
#define SPIxBRG_MASK		0x1ff
#define SPIxBRG_SHIFT		0x0

/* Bit fields in SPIxCON2 Register */
#define SPI_INT_TX_UR_EN	0x0400 /* Enable int on Tx under-run */
#define SPI_INT_RX_OV_EN	0x0800 /* Enable int on Rx over-run */
#define SPI_INT_FRM_ERR_EN	0x1000 /* Enable frame err int */

/* Rx-fifo state for RX interrupt generation */
#define SPI_RX_FIFO_EMTPY	0x0
#define SPI_RX_FIFO_NOT_EMPTY	0x1 /* not empty */
#define SPI_RX_FIFO_HALF_FULL	0x2 /* full by one-half or more */
#define SPI_RX_FIFO_FULL	0x3 /* completely full */

/* TX-fifo state for TX interrupt generation */
#define SPI_TX_FIFO_ALL_EMPTY	0x0 /* completely empty */
#define SPI_TX_FIFO_EMTPY	0x1 /* empty */
#define SPI_TX_FIFO_HALF_EMPTY	0x2 /* empty by half or more */
#define SPI_TX_FIFO_NOT_FULL	0x3 /* atleast one empty */

/* Transfer bits-per-word */
#define SPI_BPW_8		0x0
#define SPI_BPW_16		0x1
#define SPI_BPW_32		0x2

/* SPI clock sources */
#define SPI_CLKSRC_PBCLK	0x0
#define SPI_CLKSRC_MCLK		0x1

struct pic32_spi {
	void __iomem		*regs;
	int			fault_irq;
	int			rx_irq;
	int			tx_irq;
	u32			fifo_n_byte; /* FIFO depth in bytes */
	struct clk		*clk;
	spinlock_t		lock;
	unsigned long		irq_flags;
	struct spi_master	*master;

	/* Current SPI device specific */
	struct spi_device	*spi_dev;
	u32			speed_hz; /* spi-clk rate */
#define SPI_XFER_POLL	BIT(1)  /* PIO Transfer based on polling */
#define SPI_SS_MASTER	BIT(2)	/* SPI master driven SPI SS */
	u32			flags;
	u8			fifo_n_elm; /* max elements fifo can hold */

	/* Current message/transfer state */
	struct spi_message	*mesg;
	const void		*tx;
	const void		*tx_end;
	const void		*rx;
	const void		*rx_end;
	struct completion	xfer_done;

	/* SPI FiFo accessor */
	void (*rx_fifo)(struct pic32_spi *);
	void (*tx_fifo)(struct pic32_spi *);
};

static inline void spi_enable_fifo(struct pic32_spi *pic32s)
{
	/* In enhanced buffer mode fifo-depth is fixed to 128bit(= 16B) */
	writel(SPIxCON_ENHBUF, pic32s->regs + SPIxCON_SET);
	pic32s->fifo_n_byte = 16;
}

static inline u32 spi_rx_fifo_level(struct pic32_spi *pic32s)
{
	u32 sr = readl(pic32s->regs + SPIxSTAT);
	return (sr >> STAT_RF_LVL_SHIFT) & STAT_RF_LVL_MASK;
}

static inline u32 spi_tx_fifo_level(struct pic32_spi *pic32s)
{
	u32 sr = readl(pic32s->regs + SPIxSTAT);
	return (sr >> STAT_TF_LVL_SHIFT) & STAT_TF_LVL_MASK;
}

static inline void spi_enable_master(struct pic32_spi *pic32s)
{
	writel(SPIxCON_MSTEN, pic32s->regs + SPIxCON_SET);
}

static inline void spi_enable_chip(struct pic32_spi *pic32s)
{
	writel(SPIxCON_ON, pic32s->regs + SPIxCON_SET);
}

static inline void spi_disable_chip(struct pic32_spi *pic32s)
{
	writel(SPIxCON_ON, pic32s->regs + SPIxCON_CLR);
	cpu_relax();
}

static inline void spi_set_clk_mode(struct pic32_spi *pic32s, int mode)
{
	u32 conset = 0, conclr = 0;

	if (mode & SPI_CPOL)  /* active low */
		conset |= SPIxCON_CKP;
	else
		conclr |= SPIxCON_CKP;

	if (mode & SPI_CPHA) /* tx on rising edge of clk */
		conclr |= SPIxCON_CKE;
	else
		conset |= SPIxCON_CKE;

	/* rx at end of tx */
	conset |= SPIxCON_SMP;

	writel(conclr, pic32s->regs + SPIxCON_CLR);
	writel(conset, pic32s->regs + SPIxCON_SET);
}

static inline void spi_set_ws(struct pic32_spi *pic32s, int ws)
{
	writel(SPIxCON_BPW << SPIxCON_BPW_SHIFT, pic32s->regs + SPIxCON_CLR);
	writel(ws << SPIxCON_BPW_SHIFT, pic32s->regs + SPIxCON_SET);
}

static inline void spi_drain_rx_buf(struct pic32_spi *pic32s)
{
	u32 sr;

	/* drain rx bytes until empty */
	for (;;) {
		sr = readl(pic32s->regs + SPIxSTAT);
		if (sr & STAT_RF_EMPTY)
			break;

		(void)readl(pic32s->regs + SPIxBUF);
	}

	/* clear rx overflow */
	writel(STAT_RX_OV, pic32s->regs + SPIxSTAT_CLR);
}

static inline void spi_set_clk_rate(struct pic32_spi *pic32s, u32 sck)
{
	u16 clk_div;

	/* sck = clk_in / [2 * (clk_div + 1)]
	 * ie. clk_div = [clk_in / (2 * sck)] - 1
	 */
	clk_div = (clk_get_rate(pic32s->clk) / (2 * sck)) - 1;
	clk_div &= SPIxBRG_MASK;

	/* apply baud */
	writel(clk_div, pic32s->regs + SPIxBRG);
}

static inline void spi_set_clk(struct pic32_spi *pic32s, int clk_id)
{
	switch (clk_id) {
	case SPI_CLKSRC_PBCLK:
		writel(SPIxCON_MCLKSEL, pic32s->regs + SPIxCON_CLR);
		break;
	case SPI_CLKSRC_MCLK:
		writel(SPIxCON_MCLKSEL, pic32s->regs + SPIxCON_SET);
		break;
	}
}

static inline void spi_clear_rx_fifo_overflow(struct pic32_spi *pic32s)
{
	writel(STAT_RX_OV, pic32s->regs + SPIxSTAT_CLR);
}

static inline void spi_set_ss_auto(struct pic32_spi *pic32s, u16 mst_driven)
{
	/* spi controller can drive CS/SS during transfer depending on fifo
	 * fill-level. SS will stay asserted as long as TX fifo has something
	 * to transfer, else will be deasserted confirming completion of
	 * the ongoing transfer.
	 */
	if (mst_driven)
		writel(SPIxCON_MSSEN, pic32s->regs + SPIxCON_SET);
	else
		writel(SPIxCON_MSSEN, pic32s->regs + SPIxCON_CLR);
}

static inline void spi_set_rx_intr(struct pic32_spi *pic32s, int rx_fifo_state)
{
	writel(0x3 << SPIxCON_RXI_SHIFT, pic32s->regs + SPIxCON_CLR);
	writel(rx_fifo_state << SPIxCON_RXI_SHIFT, pic32s->regs + SPIxCON_SET);
}

static inline void spi_set_tx_intr(struct pic32_spi *pic32s, int tx_fifo_state)
{
	writel(0x3 << SPIxCON_TXI_SHIFT, pic32s->regs + SPIxCON_CLR);
	writel(tx_fifo_state << SPIxCON_TXI_SHIFT, pic32s->regs + SPIxCON_SET);
}

static inline void spi_set_err_int(struct pic32_spi *pic32s)
{
	writel(SPI_INT_TX_UR_EN|SPI_INT_RX_OV_EN|SPI_INT_FRM_ERR_EN,
		pic32s->regs + SPIxCON2_SET);
}

static inline void spi_disable_frame_mode(struct pic32_spi *pic32s)
{
	writel(SPIxCON_FRMEN, pic32s->regs + SPIxCON_CLR);
}

static inline void spi_spin_lock(struct pic32_spi *pic32s)
{
	spin_lock_irqsave(&pic32s->lock, pic32s->irq_flags);
}

static inline void spi_spin_unlock(struct pic32_spi *pic32s)
{
	spin_unlock_irqrestore(&pic32s->lock, pic32s->irq_flags);
}

/* Return the max entries we can fill into tx fifo */
static inline u32 pic32_tx_max(struct pic32_spi *pic32s, int n_bytes)
{
	u32 tx_left, tx_room, rxtx_gap;

	tx_left = (pic32s->tx_end - pic32s->tx) / n_bytes;
	tx_room = pic32s->fifo_n_elm - spi_tx_fifo_level(pic32s);

	/*
	 * Another concern is about the tx/rx mismatch, we
	 * though to use (pic32s->fifo_n_byte - rxfl - txfl) as
	 * one maximum value for tx, but it doesn't cover the
	 * data which is out of tx/rx fifo and inside the
	 * shift registers. So a control from sw point of
	 * view is taken.
	 */
	rxtx_gap = ((pic32s->rx_end - pic32s->rx) -
		    (pic32s->tx_end - pic32s->tx)) / n_bytes;
	return min3(tx_left, tx_room, (u32) (pic32s->fifo_n_elm - rxtx_gap));
}

/* Return the max entries we should read out of rx fifo */
static inline u32 pic32_rx_max(struct pic32_spi *pic32s, int n_bytes)
{
	u32 rx_left = (pic32s->rx_end - pic32s->rx) / n_bytes;

	return min_t(u32, rx_left, spi_rx_fifo_level(pic32s));
}

#define BUILD_SPI_FIFO_RW(__name, __type, __bwl)		\
static void pic32_spi_rx_##__name(struct pic32_spi *pic32s)	\
{								\
	__type v;						\
	u32 mx = pic32_rx_max(pic32s, sizeof(__type));		\
	for (; mx; mx--) {					\
		v = read##__bwl(pic32s->regs + SPIxBUF);	\
		*(__type *)(pic32s->rx) = v;			\
		pic32s->rx += sizeof(__type);			\
	}							\
}								\
								\
static void pic32_spi_tx_##__name(struct pic32_spi *pic32s)	\
{								\
	__type v;						\
	u32 mx = pic32_tx_max(pic32s, sizeof(__type));		\
	for (; mx ; mx--) {					\
		v = *(__type *)(pic32s->tx);			\
		write##__bwl(v, pic32s->regs + SPIxBUF);	\
		pic32s->tx += sizeof(__type);			\
	}							\
}
BUILD_SPI_FIFO_RW(byte, u8, b);
BUILD_SPI_FIFO_RW(word, u16, w);
BUILD_SPI_FIFO_RW(dword, u32, l);

static void pic32_err_stop(struct pic32_spi *pic32s, const char *msg)
{
	/* Stop the hw */
	spi_disable_chip(pic32s);

	/* Show err message and abort xfer with err */
	dev_err(&pic32s->master->dev, "%s\n", msg);
	pic32s->mesg->state = (void *)-1;
	complete(&pic32s->xfer_done);

	/* disable all interrupts */
	disable_irq_nosync(pic32s->fault_irq);
	disable_irq_nosync(pic32s->rx_irq);
	disable_irq_nosync(pic32s->tx_irq);
	dev_err(&pic32s->master->dev, "irq: disable all\n");
}

static irqreturn_t pic32_spi_fault_irq(int irq, void *dev_id)
{
	struct pic32_spi *pic32s = dev_id;
	u32 status = readl(pic32s->regs + SPIxSTAT);

	spin_lock(&pic32s->lock);

	/* Error handling */
	if (status & (STAT_RX_OV | STAT_FRM_ERR | STAT_TX_UR)) {
		writel(STAT_RX_OV, pic32s->regs + SPIxSTAT_CLR);
		writel(STAT_TX_UR, pic32s->regs + SPIxSTAT_CLR);
		pic32_err_stop(pic32s, "err_irq: fifo ov/ur-run\n");
		goto irq_handled;
	}

	if (status & STAT_FRM_ERR) {
		pic32_err_stop(pic32s, "err_irq: frame error");
		goto irq_handled;
	}

	if (!pic32s->mesg) {
		pic32_err_stop(pic32s, "err_irq: mesg error");
		goto irq_handled;
	}

irq_handled:
	spin_unlock(&pic32s->lock);

	return IRQ_HANDLED;
}

static irqreturn_t pic32_spi_rx_irq(int irq, void *dev_id)
{
	struct pic32_spi *pic32s = dev_id;

	spin_lock(&pic32s->lock);

	pic32s->rx_fifo(pic32s);

	/* rx complete? */
	if (pic32s->rx_end == pic32s->rx) {
		/* mask & disable all interrupts */
		disable_irq_nosync(pic32s->fault_irq);
		disable_irq_nosync(pic32s->rx_irq);

		/* complete current xfer */
		complete(&pic32s->xfer_done);
	}

	spin_unlock(&pic32s->lock);

	return IRQ_HANDLED;
}

static irqreturn_t pic32_spi_tx_irq(int irq, void *dev_id)
{
	struct pic32_spi *pic32s = dev_id;
	spin_lock(&pic32s->lock);

	pic32s->tx_fifo(pic32s);

	/* tx complete? mask and disable tx interrupt */
	if (pic32s->tx_end == pic32s->tx)
		disable_irq_nosync(pic32s->tx_irq);

	spin_unlock(&pic32s->lock);

	return IRQ_HANDLED;
}

static inline void pic32_spi_cs_assert(struct pic32_spi *pic32s)
{
	int active;
	struct spi_device *spi_dev = pic32s->spi_dev;

	if (pic32s->flags & SPI_SS_MASTER)
		return;

	active = spi_dev->mode & SPI_CS_HIGH;
	gpio_set_value(spi_dev->cs_gpio, active);
}

static inline void pic32_spi_cs_deassert(struct pic32_spi *pic32s)
{
	int active;
	struct spi_device *spi_dev = pic32s->spi_dev;

	if (pic32s->flags & SPI_SS_MASTER)
		return;

	active = spi_dev->mode & SPI_CS_HIGH;
	gpio_set_value(spi_dev->cs_gpio, !active);
}

static int pic32_poll_transfer(struct pic32_spi *pic32s, unsigned long timeout)
{
	unsigned long deadline;

	deadline = timeout + jiffies;
	for (;;) {
		pic32s->tx_fifo(pic32s);
		cpu_relax();
		pic32s->rx_fifo(pic32s);
		cpu_relax();

		/* received sufficient data */
		if (pic32s->rx >= pic32s->rx_end)
			break;

		/* timedout ? */
		if (time_after_eq(jiffies, deadline))
			return -EIO;
	}

	return 0;
}

static void pic32_spi_set_word_size(struct pic32_spi *pic32s, u8 bpw)
{
	u8 spi_bpw;

	switch (bpw) {
	default:
	case 8:
		pic32s->rx_fifo = pic32_spi_rx_byte;
		pic32s->tx_fifo = pic32_spi_tx_byte;
		spi_bpw = SPI_BPW_8;
		break;
	case 16:
		pic32s->rx_fifo = pic32_spi_rx_word;
		pic32s->tx_fifo = pic32_spi_tx_word;
		spi_bpw = SPI_BPW_16;
		break;
	case 32:
		pic32s->rx_fifo = pic32_spi_rx_dword;
		pic32s->tx_fifo = pic32_spi_tx_dword;
		spi_bpw = SPI_BPW_32;
		break;
	}

	/* calculate maximum elements fifo can hold */
	pic32s->fifo_n_elm = DIV_ROUND_UP(pic32s->fifo_n_byte, (bpw >> 3));

	/* set bits per word */
	spi_set_ws(pic32s, spi_bpw);
}

static int pic32_spi_one_transfer(struct pic32_spi *pic32s,
				  struct spi_message *message,
				  struct spi_transfer *transfer)
{
	int ret = 0;

	/* set current transfer information */
	pic32s->tx = (const void *)transfer->tx_buf;
	pic32s->rx = (const void *)transfer->rx_buf;
	pic32s->tx_end = pic32s->tx + transfer->len;
	pic32s->rx_end = pic32s->rx + transfer->len;

	/* Handle per transfer options for speed */
	if (transfer->speed_hz && (transfer->speed_hz != pic32s->speed_hz)) {
		spi_set_clk_rate(pic32s, transfer->speed_hz);
		pic32s->speed_hz = transfer->speed_hz;
	}

	/* handle bits-per-word */
	if (transfer->bits_per_word)
		pic32_spi_set_word_size(pic32s, transfer->bits_per_word);

	/* enable chip */
	spi_enable_chip(pic32s);

	/* polling mode? */
	if (pic32s->flags & SPI_XFER_POLL) {
		ret = pic32_poll_transfer(pic32s, 2 * HZ);
		if (ret) {
			dev_err(&pic32s->master->dev, "poll-xfer timedout\n");
			message->status = ret;
			goto err_xfer_done;
		}
		goto out_xfer_done;
	}

	/* configure rx interrupt */
	spi_set_rx_intr(pic32s, SPI_RX_FIFO_NOT_EMPTY);

	/* enable all interrupts */
	enable_irq(pic32s->fault_irq);
	enable_irq(pic32s->tx_irq);
	enable_irq(pic32s->rx_irq);

	/* wait for completion */
	reinit_completion(&pic32s->xfer_done);
	spi_spin_unlock(pic32s);
	ret = wait_for_completion_timeout(&pic32s->xfer_done, 2 * HZ);
	spi_spin_lock(pic32s);

	if (ret <= 0) {
		dev_err(&pic32s->master->dev, "wait timedout/interrupted\n");
		message->status = ret = -EIO;
		goto err_xfer_done;
	}

out_xfer_done:
	/* Update total byte transferred */
	message->actual_length += transfer->len;
	ret = 0;

err_xfer_done:
	return ret;
}

static int pic32_spi_one_message(struct spi_master *master,
				 struct spi_message *msg)
{
	int ret = 0;
	struct pic32_spi *pic32s;
	struct spi_transfer *xfer;
	struct spi_device *spi = msg->spi;

	dev_vdbg(&spi->dev, "new message %p submitted for %s\n",
		 msg, dev_name(&spi->dev));

	pic32s = spi_master_get_devdata(master);
	pic32s->mesg = msg;

	/* disable chip */
	spi_disable_chip(pic32s);

	msg->status = 0;
	msg->state = (void *) -2; /* running */
	msg->actual_length = 0;

#if 0 /* debug msg */
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		dev_vdbg(&spi->dev, "  xfer %p: len %u tx %p rx %p\n",
			 xfer, xfer->len, xfer->tx_buf, xfer->rx_buf);
		print_hex_dump(KERN_DEBUG, "tx_buf", DUMP_PREFIX_ADDRESS,
			       16, 1, xfer->tx_buf, min_t(u32, xfer->len, 16),
			       1);
	}
#endif

	/* interrupt lock */
	spi_spin_lock(pic32s);

	/* assert cs */
	pic32_spi_cs_assert(pic32s);

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		ret = pic32_spi_one_transfer(pic32s, msg, xfer);
		if (ret) {
			dev_err(&spi->dev, "xfer %p err\n", xfer);
			goto xfer_done;
		}

		/* handle delay, if asked */
		if (xfer->delay_usecs) {
			spi_spin_unlock(pic32s);
			udelay(xfer->delay_usecs);
			spi_spin_lock(pic32s);
		}
	}

	/* update msg status */
	msg->state = 0;
	msg->status = 0;

xfer_done:
	/* deassert cs */
	pic32_spi_cs_deassert(pic32s);

	/* disable chip */
	spi_disable_chip(pic32s);

	/* interrupt unlock */
	spi_spin_unlock(pic32s);

	spi_finalize_current_message(spi->master);

	return ret;
}

/* This may be called twice for each spi dev */
static int pic32_spi_setup(struct spi_device *spi)
{
	struct pic32_spi *pic32s;
	int ret;

	pic32s = spi_master_get_devdata(spi->master);

	/* SPI master supports only one spi-device at a time.
	 * So mutiple spi_setup() to different devices is not allowed.
	 */
	if (unlikely(pic32s->spi_dev)) {
		dev_err(&spi->dev, "spi-master already associated with %s\n",
			dev_name(&pic32s->spi_dev->dev));
		return -EPERM;
	}
	pic32s->spi_dev = spi;

	/* set POLL mode, if invalid irq is provided */
	if ((pic32s->fault_irq <= 0) || (pic32s->rx_irq <= 0)
	    || (pic32s->tx_irq <= 0))
		pic32s->flags |= SPI_XFER_POLL;

	if (pic32s->flags & SPI_XFER_POLL)
		dev_info(&spi->dev, "enabled polling\n");

	/* disable chip */
	spi_disable_chip(pic32s);

	/* check word size */
	if (!spi->bits_per_word) {
		dev_err(&spi->dev, "No bits_per_word defined\n");
		return -EINVAL;
	}

	/* check maximum SPI clk rate */
	if (!spi->max_speed_hz) {
		dev_err(&spi->dev, "No max speed HZ parameter\n");
		return -EINVAL;
	}

	/* set word size */
	pic32_spi_set_word_size(pic32s, spi->bits_per_word);

	/* set spi-clk rate */
	pic32s->speed_hz = spi->max_speed_hz;
	spi_set_clk_rate(pic32s, spi->max_speed_hz);

	/* set spi xfer mode */
	spi_set_clk_mode(pic32s, spi->mode);

	/* configure master-ctl CS assert/deassert, if cs_gpio is invalid */
	pic32s->flags |= SPI_SS_MASTER;

	if (gpio_is_valid(spi->cs_gpio)) {
		/* prepare CS GPIO */
		ret = devm_gpio_request(&spi->dev,
				       spi->cs_gpio, dev_name(&spi->dev));
		if (!ret) {
			gpio_direction_output(spi->cs_gpio,
					      !(spi->mode & SPI_CS_HIGH));
			pic32s->flags &= ~SPI_SS_MASTER;
			dev_info(&spi->dev,
				"gpio%d enabled for SPI-CS\n", spi->cs_gpio);
		}
	}
	spi_set_ss_auto(pic32s, pic32s->flags & SPI_SS_MASTER);

	dev_vdbg(&spi->master->dev,
		 "successfully registered spi-device %s\n",
		 dev_name(&spi->dev));
	return 0;
}

static void pic32_spi_cleanup(struct spi_device *spi)
{
	struct pic32_spi *pic32s;

	pic32s = spi_master_get_devdata(spi->master);

	/* diasable chip */
	spi_disable_chip(pic32s);

	/* release cs-gpio */
	if (!(pic32s->flags & SPI_SS_MASTER))
		devm_gpio_free(&spi->dev, spi->cs_gpio);

	/* reset reference */
	pic32s->spi_dev = NULL;
	pic32s->speed_hz = 0;
}

static void pic32_spi_hw_init(struct pic32_spi *pic32s)
{
	/* disable module */
	spi_disable_chip(pic32s);

	/* drain rx buf */
	spi_drain_rx_buf(pic32s);

	/* enable enhanced buffer mode */
	spi_enable_fifo(pic32s);

	/* clear rx overflow indicator */
	spi_clear_rx_fifo_overflow(pic32s);

	/* disable frame synchronization mode */
	spi_disable_frame_mode(pic32s);

	/* enable master mode while disabled */
	spi_enable_master(pic32s);

	/* tx fifo interrupt threshold */
	spi_set_tx_intr(pic32s, SPI_TX_FIFO_HALF_EMPTY);

	/* rx fifo interrupt threshold */
	spi_set_rx_intr(pic32s, SPI_RX_FIFO_NOT_EMPTY);

	/* report error though interrupt */
	spi_set_err_int(pic32s);
}

static int pic32_spi_hw_probe(struct platform_device *pdev,
			      struct pic32_spi *pic32s)
{
	int ret, clk_id = 0;
	struct resource *mem;
	struct device_node *np;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "mem resource not found\n");
		return -ENOENT;
	}

	pic32s->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (!pic32s->regs) {
		dev_err(&pdev->dev, "mem map failed\n");
		return -ENOMEM;
	}

	/* get irq resources: err-irq, rx-irq, tx-irq */
	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		dev_err(&pdev->dev, "get fault-irq failed\n");
	pic32s->fault_irq = ret;

	ret = platform_get_irq(pdev, 1);
	if (ret < 0)
		dev_err(&pdev->dev, "get rx-irq failed\n");
	pic32s->rx_irq = ret;

	ret = platform_get_irq(pdev, 2);
	if (ret < 0)
		dev_err(&pdev->dev, "get tx-irq map failed\n");
	pic32s->tx_irq = ret;

	/* Basic HW init */
	pic32_spi_hw_init(pic32s);

	/* two possible clksrcs; pbxclk:0, m_clk:1 */
	np = pdev->dev.of_node;
	if (np) {
		ret = of_clk_get_parent_count(np);
		if ((ret < 1) || (ret > 2)) {
			dev_err(&pdev->dev, "clk not specified.\n");
			goto err_unmap_mem;
		}

		/* get active clk */
		of_property_read_u32(np, "microchip,clock-indices", &clk_id);
		pic32s->clk = of_clk_get(np, clk_id);
	} else {
		pic32s->clk = devm_clk_get(&pdev->dev, NULL);
	}

	if (IS_ERR(pic32s->clk)) {
		ret = PTR_ERR(pic32s->clk);
		dev_err(&pdev->dev, "clk get failed\n");
		goto err_unmap_mem;
	}

	clk_prepare_enable(pic32s->clk);

	/* Select clk src */
	spi_set_clk(pic32s, clk_id);

	spin_lock_init(&pic32s->lock);
	return 0;

err_unmap_mem:
	dev_err(&pdev->dev, "hw_probe failed, ret %d\n", ret);
	devm_iounmap(&pdev->dev, pic32s->regs);
	return ret;
}

static int pic32_spi_probe(struct platform_device *pdev)
{
	int ret;
	struct spi_master *master;
	struct pic32_spi *pic32s;
	struct device *dev = &pdev->dev;
	u32 max_spi_rate = 50000000;

	master = spi_alloc_master(dev, sizeof(*pic32s));
	if (!master)
		return -ENOMEM;

	pic32s = spi_master_get_devdata(master);
	pic32s->master = master;

	ret = pic32_spi_hw_probe(pdev, pic32s);
	if (ret) {
		dev_err(&pdev->dev, "spi-hw probe failed.\n");
		goto err_free_master;
	}

	if (dev->of_node)
		of_property_read_u32(dev->of_node,
				     "max-clock-frequency", &max_spi_rate);

	master->dev.of_node	= of_node_get(pdev->dev.of_node);
	master->mode_bits	= SPI_MODE_3|SPI_MODE_0;
	master->num_chipselect	= 1;
	master->max_speed_hz	= max_spi_rate;
	master->setup		= pic32_spi_setup;
	master->cleanup		= pic32_spi_cleanup;
	master->flags		= SPI_MASTER_MUST_TX|SPI_MASTER_MUST_RX;
	master->bits_per_word_mask	= SPI_BPW_RANGE_MASK(8, 32);
	master->transfer_one_message	= pic32_spi_one_message;

	init_completion(&pic32s->xfer_done);

	/* install irq handlers (with irq-disabled) */
	irq_set_status_flags(pic32s->fault_irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(dev, pic32s->fault_irq,
			       pic32_spi_fault_irq, IRQF_NO_THREAD,
			       dev_name(dev), pic32s);
	if (ret < 0) {
		dev_warn(&pdev->dev, "request fault-irq %d\n", pic32s->rx_irq);
		pic32s->flags |= SPI_XFER_POLL;
		goto irq_request_done;
	}

	/* receive interrupt handler */
	irq_set_status_flags(pic32s->rx_irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(dev, pic32s->rx_irq,
			       pic32_spi_rx_irq, IRQF_NO_THREAD, dev_name(dev),
			       pic32s);
	if (ret < 0) {
		dev_warn(&pdev->dev, "request rx-irq %d\n", pic32s->rx_irq);
		pic32s->flags |= SPI_XFER_POLL;
		goto irq_request_done;
	}

	/* transmit interrupt handler */
	irq_set_status_flags(pic32s->tx_irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(dev, pic32s->tx_irq,
			       pic32_spi_tx_irq, IRQF_NO_THREAD, dev_name(dev),
			       pic32s);
	if (ret < 0) {
		dev_warn(&pdev->dev, "request tx-irq %d\n", pic32s->tx_irq);
		pic32s->flags |= SPI_XFER_POLL;
		goto irq_request_done;
	}

irq_request_done:
	/* register master */
	ret = devm_spi_register_master(dev, master);
	if (ret) {
		dev_err(&master->dev, "failed registering spi master\n");
		goto err_hw_remove;
	}

	platform_set_drvdata(pdev, pic32s);

	return 0;

err_hw_remove:
	/* disable hw */
	spi_disable_chip(pic32s);

	/* disable clk */
	if (!IS_ERR_OR_NULL(pic32s->clk))
		clk_disable_unprepare(pic32s->clk);
err_free_master:
	spi_master_put(master);
	return ret;
}

static int pic32_spi_remove(struct platform_device *pdev)
{
	struct pic32_spi *pic32s;

	pic32s = platform_get_drvdata(pdev);

	/* disable hw */
	spi_disable_chip(pic32s);

	/* disable clk */
	clk_disable_unprepare(pic32s->clk);
	return 0;
}

static const struct of_device_id pic32_spi_of_match[] = {
	{.compatible = "microchip,pic32-spi",},
	{},
};
MODULE_DEVICE_TABLE(of, pic32_spi_of_match);

static struct platform_driver pic32_spi_driver = {
	.driver = {
		.name = "spi-pic32",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pic32_spi_of_match),
	},
	.probe = pic32_spi_probe,
	.remove = pic32_spi_remove,
};

module_platform_driver(pic32_spi_driver);

MODULE_AUTHOR("Purna Chandra Mandal <purna.mandal@microchip.com>");
MODULE_DESCRIPTION("Microchip SPI driver for PIC32 SPI controller.");
MODULE_LICENSE("GPL v2");
