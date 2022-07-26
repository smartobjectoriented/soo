/**
 * soo_heat_base.c
 * Thomas Rieder
 * Décomposition en nombre premier, affichage sur 7seg 
 **/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/wait.h>

#include <linux/mutex.h>

#include <asm/termios.h>
#include <linux/serial_core.h>
#include <linux/tty.h>


#define TEMP_BLOCK_SIZE 3
#define DEV_ID_BLOCK_SIZE 4
#define DEV_TYPE_BLOCK_SIZE 1

//Licence utilisée pour le driver
MODULE_LICENSE("GPL");

//Auteur du driver
MODULE_AUTHOR("Thomas Rieder");

//descriptioin du driver
MODULE_DESCRIPTION("Driver soo_heat_base");

extern struct tty_struct *tty_kopen(dev_t device);
extern void uart_do_open(struct tty_struct *tty);
extern int tty_set_termios(struct tty_struct *tty, struct ktermios *new_termios);
extern int uart_do_write(struct tty_struct *tty, const unsigned char *buf, int count);
extern void n_tty_do_flush_buffer(struct tty_struct *tty);
extern ssize_t tty_do_read(struct tty_struct *tty, unsigned char *buf, size_t nr);





typedef struct {
	struct tty_struct *tty_uart;
	struct mutex mutex_uart;
	bool tty_is_open;
} soo_base_priv_t;

soo_base_priv_t soo_base_priv;

/**
 * \brief write in uart1
 **/
ssize_t soo_heat_base_write_cmd(char *buffer, ssize_t len) {

	char buff_send[25];

	printk("Buffer from ME = %s\n", buffer);

	sprintf(buff_send, "radio tx %s\r\n", buffer);

	mutex_lock(&soo_base_priv.mutex_uart);

	uart_do_write(soo_base_priv.tty_uart, "mac pause\r\n", 11); //Sortie du mode LoRaWAN 
	msleep(100);


	/* send command to LoRa module */
	uart_do_write(soo_base_priv.tty_uart, buff_send, len+11);
	msleep(100);


            

    /* flush the buffer of all unwanted responses */
	// n_tty_do_flush_buffer(soo_base_priv.tty_uart);
	// msleep(100);

	mutex_unlock(&soo_base_priv.mutex_uart);
	

    return len+11;
}
EXPORT_SYMBOL_GPL(soo_heat_base_write_cmd);


/**
 * \brief read from uart1
 **/
ssize_t soo_heat_base_read_temp(char *buffer) {
	

    int len;
    int nbytes = 0;
    int bytes_to_read = 18;    


	// mutex_lock(&soo_base_priv.mutex_uart);

    /* flush the buffer of all unwanted responses */
	n_tty_do_flush_buffer(soo_base_priv.tty_uart);

    while(nbytes < bytes_to_read) {

        /* read responses byte by byte to check if we get
		   an unwanted message from LoRa module like "ok", "invalid_param"*/
		/* maybe there is a better way ? */
        len = tty_do_read(soo_base_priv.tty_uart, buffer + nbytes, 1);
        nbytes += len;
        
        buffer[nbytes] = '\0';

        printk("SOO_HEAT_BASE Current read : %s\n", buffer);

        /* check for unwanted responses just to be sure */
        if(strstr(buffer, "ok") != NULL || 
            strstr(buffer, "invalid_param") != NULL) {
            
            nbytes = 0;
            memset(buffer, 0, 18);
            printk("SOO_HEAT_BASE Reset read, unwanted response !!\n");
        }

    }

    // printk("SOO_HEAT_BASE READED %s\n", buffer);

    n_tty_do_flush_buffer(soo_base_priv.tty_uart);


	/* disable LoRaWAN*/
	uart_do_write(soo_base_priv.tty_uart, "mac pause\r\n", 11);
	msleep(50);

	/* set continue listening */
	uart_do_write(soo_base_priv.tty_uart, "radio rx 0\r\n", 12);
	msleep(50);

    /* flush the buffer of all unwanted responses */
	n_tty_do_flush_buffer(soo_base_priv.tty_uart);

	// mutex_unlock(&soo_base_priv.mutex_uart);

    return nbytes;
}
EXPORT_SYMBOL_GPL(soo_heat_base_read_temp);


/**
 * \brief setting the LoRa module on reception mode
 **/
void soo_heat_base_set_lora_rx(void){
    
	mutex_lock(&soo_base_priv.mutex_uart);
    /* reset the module to be safe */
	// uart_do_write(soo_base_priv.tty_uart, "sys reset\r\n", 11);
	// msleep(100);

	/* Setup the LoRa module */
	printk("SOO_HEAT_BASE Setting LoRa module... \n");


	/* set modulation type*/
	uart_do_write(soo_base_priv.tty_uart, "radio set mod lora\r\n", 20);
	msleep(100);

	/* set frequence to 915MHz*/
	uart_do_write(soo_base_priv.tty_uart, "radio set freq 868000000\r\n", 26);
	msleep(100);

	/* set emission power */
	uart_do_write(soo_base_priv.tty_uart, "radio set pwr 5\r\n", 17);
	msleep(100);

	/* set unlimited timeout limit */
	uart_do_write(soo_base_priv.tty_uart, "radio set wdt 0\r\n", 17);
	msleep(100);

	/* disable LoRaWAN*/
	uart_do_write(soo_base_priv.tty_uart, "mac pause\r\n", 11);
	msleep(100);

	/* set continue listening */
	uart_do_write(soo_base_priv.tty_uart, "radio rx 0\r\n", 12);
	msleep(100);

	printk("SOO_HEAT_BASE Setting LoRa module finish \n");

    /* flush the buffer of all unwanted responses */
	n_tty_do_flush_buffer(soo_base_priv.tty_uart);

	mutex_unlock(&soo_base_priv.mutex_uart);
}
EXPORT_SYMBOL_GPL(soo_heat_base_set_lora_rx);


void setup_lora_module(void) {

    /* reset the module to be safe */
	// uart_do_write(soo_base_priv.tty_uart, "sys reset\r\n", 11);
	// msleep(100);

	/* Setup the LoRa module */
	printk("SOO_HEAT_BASE Setup LoRa module... \n");

	/* get version */
	// uart_do_write(soo_base_priv.tty_uart, "sys get ver\r\n", 13);
	// msleep(100);

	/* set modulation type */
	uart_do_write(soo_base_priv.tty_uart, "radio set mod lora\r\n", 20);
	msleep(100);

	/* set freq*/
	uart_do_write(soo_base_priv.tty_uart, "radio set freq 868000000\r\n", 26);
	msleep(100);

	/* set emission power */
	uart_do_write(soo_base_priv.tty_uart, "radio set pwr 5\r\n", 17);
	msleep(100);

	/* set unlimited timeout limit */
	uart_do_write(soo_base_priv.tty_uart, "radio set wdt 0\r\n", 17);
	msleep(100);

	/* get freq */
	uart_do_write(soo_base_priv.tty_uart, "radio get freq\r\n", 16);
	msleep(100);

	/* disable LoRaWAN*/
	uart_do_write(soo_base_priv.tty_uart, "mac pause\r\n", 11);
	msleep(100);
}


/**
 * @brief open given uart tty port thqat need synchronized access
 * @return -1 if already open, 0 on success
 **/
int soo_heat_base_open(const char *port) {

	dev_t dev;
	int baud = 57600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';


	if(soo_base_priv.tty_is_open) {
		return -1;
	}

    tty_dev_name_to_number(port, &dev);
	soo_base_priv.tty_uart = tty_kopen(dev);

    printk("SOO_HEAT_BASE Open uart \n");
	uart_do_open(soo_base_priv.tty_uart);

	soo_base_priv.tty_is_open = true;


	printk("SOO_HEAT_BASE tty_set_termios....\n");

	/* Set the termios parameters related to tty. */
	soo_base_priv.tty_uart->termios.c_iflag = (IUTF8 | IMAXBEL | IUCLC | IXANY);
	soo_base_priv.tty_uart->termios.c_oflag = ~(OPOST);
	soo_base_priv.tty_uart->termios.c_cflag = (CREAD | CMSPAR);
	soo_base_priv.tty_uart->termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON | IEXTEN | ISIG | NOFLSH);
	soo_base_priv.tty_uart->termios.c_lflag |= ICANON;
	// soo_base_priv.tty_uart->termios.c_lflag = NOFLSH;

	tty_set_termios(soo_base_priv.tty_uart, &soo_base_priv.tty_uart->termios);

	printk("SOO_HEAT_BASE TTY Set option\n");
	uart_set_options(((struct uart_state *) soo_base_priv.tty_uart->driver_data)->uart_port, NULL, baud, parity, bits, flow);

	setup_lora_module();

	return 0;
}
EXPORT_SYMBOL_GPL(soo_heat_base_open);


/**
 * \brief Fonction d'initialisation du driver
 * \param pdev structure du device
 **/
static int soo_heat_base_probe(struct platform_device *pdev) {

	printk("soo_heat_base driver probe() called !\n");


    mutex_init(&soo_base_priv.mutex_uart);

	soo_base_priv.tty_uart = kmalloc(sizeof(struct tty_struct), GFP_KERNEL);
	soo_base_priv.tty_is_open = false;


    //retourne le code d'erreur
	return 0;
}

/**
 * \brief free all ressources and remove the driver
 **/
static int soo_heat_base_remove(struct platform_device *pdev) {


	printk("Remove soo_heat_base driver !\n");

	kfree(soo_base_priv.tty_uart);

    return 0;
}


// structure contenant le compatible à matcher dans le device tree
static const struct of_device_id soo_heat_base_driver_id[] = {

	//valeur à match dans le DT
	{ .compatible = "lora,soo-heat-base" },
	{},
};

MODULE_DEVICE_TABLE(of, soo_heat_base_driver_id);

// structure contenant les informations sur le driver
// qui seront comparé au device tree afin de match avec
// un device, contient aussi les fonctions de callback
// probe et remove
static struct platform_driver soo_heat_base_driver = {
	
	.probe = soo_heat_base_probe,
	.remove = soo_heat_base_remove,
	.driver = {
		// nom du driver
		.name = "soo_heat_base-uart",
		//propriétaire du driver
		.owner = THIS_MODULE,
		//valeur à macth dans le DT
		.of_match_table = of_match_ptr(soo_heat_base_driver_id),
	},
	
};



static void __exit soo_heat_base_exit(void)
{
	platform_driver_unregister(&soo_heat_base_driver);
	printk(KERN_ALERT "Goodbye module soo_heat_base.\n");
}

module_exit(soo_heat_base_exit);


// va vérifier si un device est compatible avec ce driver
// et appellera probe() si c'est le cas
module_platform_driver(soo_heat_base_driver);
