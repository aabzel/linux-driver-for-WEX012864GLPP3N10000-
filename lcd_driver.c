/**
 * @file   lcd_driver.c
 * @author Alexander Barunin aabzel@yandex.ru
 * @date   15 Jul 2016
 * @brief 
# linux-driver-for-WEX012864GLPP3N10000
the linux driver for graphic monochrome OLED display.  

List of required hardware:
1. Beagle Bone Black board
2. WEX012864GLPP3N10000 display (ssd1305 controller )


The rule of the connection the display to BBB board.
                                                      
______________pinout____________________                            
|CON1#| Symbol|  BBB HEAD PIN | GPIO#  |                 
|_____|_______|_______________|________|              
| 1   |  VDD  |    P9.4       | DC3.3V |             
| 2   |  VSS  |    P8.1       |  DGND  |               
| 3   |  NC   |     -         |   -    |            
| 4   |  D0   |    P8.45      |   70   |               
| 5   |  D1   |    P8.46      |   71   |              
| 6   |  D2   |    P8.43      |   72   |              
| 7   |  D3   |    P8.44      |   73   |              
| 8   |  D4   |    P8.41      |   74   |               
| 9   |  D5   |    P8.42      |   75   |               
| 10  |  D6   |    P8.39      |   76   |                
| 11  |  D7   |    P8.40      |   77   |                 
| 12  |  CS#  |    P8.37      |   78   |                 
| 13  |  NC   |     -         |   -    |                   
| 14  |  RES# |    P8.38      |   79   |                
| 15  |  R/W# |    P8.36      |   80   |                
| 16  |  D/C# |    P8.34      |   81   |                    
| 17  |  E/RD#|    P8.35      |   8    |                      
| 18  |  NC   |     -         |   -    |                    
| 19  |  DISP |    P8.33      |   9    |                   
| 20  |  NC   |     -         |   -    |                   
|_____|_______|_____-_________|___-____|               
                                                       



 
 * @see 
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>                 // Required for the GPIO functions
#include <linux/interrupt.h>            // Required for the IRQ code
#include <linux/pinctrl/pinctrl.h> 
#include <linux/delay.h>      // Using this header for the msleep() function
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/string.h>  // for strlen   
#include <asm/uaccess.h>          // Required for the copy to user function


#define  DEVICE_NAME "display"    ///< The device will appear at /dev/display using this value
#define  CLASS_NAME  "ebb"        ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Barunin");
MODULE_DESCRIPTION("OLED display driver for the BBB");
MODULE_VERSION("0.1");


static char *info_str = "lcd device driver.\nAuthor: Alexander Barunin aabzel@yandex.ru\n"; 

static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   textptr[300]  = {0};
static char   message[1024] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message=0;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  ebbcharClass  = NULL; ///< The device-driver class struct pointer
static struct device* ebbcharDevice = NULL; ///< The device-driver device struct pointer

// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

   

#define DATA0_PIN    gpio70
#define DATA1_PIN    gpio71
#define DATA2_PIN    gpio72
#define DATA3_PIN    gpio73
#define DATA4_PIN    gpio74
#define DATA5_PIN    gpio75
#define DATA6_PIN    gpio76  
#define DATA7_PIN    gpio77

#define CS_PIN       gpio78
#define RES_PIN      gpio79
#define RW_PIN       gpio80
#define DC_PIN       gpio81
#define E_RD_PIN     gpio8
#define DISP_PIN     gpio9
   

#define GPIO_PIN_SET    1
#define GPIO_PIN_RESET  0


  
// COMMAND SET for SSD1305
#define SSD1305_NORMALDISPLAY 0xA6
#define SSD1305_INVERTDISPLAY 0xA7
   
#define DISPLAY_ON_IN_DIM_MODE  (0xAC)  //тусклый 

#define SET_CONTRAST            (0x81)
#define SET_BRIGHTNESS          (0x82)   
   
#define SSD1305_SETLUT 0x91   
   
#define SSD1305_SEGREMAP 0xA0   
   
#define DISPLAY_ON                  (0xAF)
#define DISPLAY_OFF                 (0xAE)
#define SET_SEGMANT_REMAP_SEG0_131  (0xA1)// segment remap 
#define SET_SEGMANT_REMAP_SEG0_0    (0xA0)// segment remap    
#define SET_DISPLAY_START_LINE      (0x7F) // 40... 7f
   
#define SET_PAGE_START 0xB0   
   
#define RIGHT_HORISONTAL_SCROLL 0x26
#define LEFT_HORISONTAL_SCROLL  0x27   
   
#define ACTIVATE_SCROLL   (0x2F)
#define DEACTIVATE_SCROLL (0x2E)



#define _BV(bit) (1 << (bit))
   
#define BLACK 0
#define WHITE 1   
   
#define SSD1305_LCDWIDTH                  128
#define SSD1305_LCDHEIGHT                 64  

typedef unsigned char bit;
typedef unsigned char BYTE;
typedef unsigned char byte;
typedef short int int6;


static byte glcd_x=0, glcd_y=0;                    // Coordinates used to specify where the next text will go
static byte glcd_size=1;                         // Same for size .Logic size of pixel
static bit glcd_colour=1;                        // Same for colour


static unsigned int gpio70 = 70;        ///<
static bool	        ledOn70 = 0;        ///<

static unsigned int gpio71 = 71;        ///<
static bool	        ledOn71 = 0;        ///<

static unsigned int gpio72 = 72;        ///<
static bool	        ledOn72 = 0;        ///<

static unsigned int gpio73 = 73;        ///<
static bool	        ledOn73 = 0;        ///<

static unsigned int gpio74 = 74;        ///<
static bool	        ledOn74 = 0;        ///<

static unsigned int gpio75 = 75;        ///<
static bool	        ledOn75 = 0;        ///<

static unsigned int gpio76 = 76;        ///<
static bool	        ledOn76 = 0;        ///<

static unsigned int gpio77 = 77;        ///<
static bool	        ledOn77 = 0;        ///<

static unsigned int gpio78 = 78;        ///<
static bool	        ledOn78 = 0;        ///<

static unsigned int gpio79 = 79;        ///<
static bool	        ledOn79 = 0;        ///<

static unsigned int gpio80 = 80;        ///<
static bool	        ledOn80 = 0;        ///<

static unsigned int gpio81 = 81;        ///<
static bool	        ledOn81 = 0;        ///<

static unsigned int gpio8  = 8;        ///<
static bool	        ledOn8 = 0;        ///<

static unsigned int gpio9  = 9;        ///<
static bool	        ledOn9 = 0;        ///<



static int numBuffers =512;


// the memory buffer for the LCD
static uint8_t buffer[SSD1305_LCDHEIGHT * SSD1305_LCDWIDTH / 8] = { 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xF0, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x0C, 0x3E, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFC, 0xFC, 0xF8, 0xF0,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x9F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xDF, 0xE0, 0xE0, 0xE0,
0xE0, 0xE0, 0xE0, 0xE0, 0xC0, 0xC0, 0xC0, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x1F, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFD, 0xFD,
0xFB, 0x7B, 0xBF, 0xFF, 0xFF, 0xFC, 0x7F, 0xFF, 0xF7, 0xF7, 0xF7, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x3F, 0x3F, 0x1F, 0x0F, 0x0F, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0xE0, 0xE0, 0xE0,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0xC0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0xE0, 0xE0, 0xE0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xF8, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFD,
0xFE, 0xFF, 0xFF, 0x3F, 0xFF, 0xFF, 0xFF, 0xFC, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFD, 0xF1,
0xC1, 0x01, 0x00, 0x00, 0x70, 0x78, 0x7C, 0x7C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0xFC, 0xFC, 0xF8,
0xF0, 0x00, 0x00, 0xF0, 0xF8, 0xFC, 0xFC, 0x3C, 0x3C, 0x3C, 0x3C, 0x78, 0xFF, 0xFF, 0xFF, 0xFF,
0x00, 0x00, 0x70, 0x78, 0x7C, 0x7C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0xFC, 0xFC, 0xF8, 0xF0, 0x00,
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x3D, 0x3D, 0x3D, 0x00, 0x00, 0xFC, 0xFC, 0xFC, 0xFC, 0x70, 0x38,
0x3C, 0x3C, 0x3C, 0x00, 0xFC, 0xFC, 0xFC, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xFC, 0xFC,
0xFC, 0x00, 0x00, 0xFC, 0xFC, 0xFC, 0xFC, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x3C, 0x3C, 0x3C,
0x00, 0x00, 0x00, 0x00, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x0F, 0x0F, 0x0F, 0x07, 0x07, 0x03,
0x03, 0x01, 0x00, 0x00, 0x00, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x3F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF,
0xFF, 0x00, 0x00, 0x00, 0xFC, 0xFE, 0xFF, 0xFF, 0xE7, 0xE7, 0xE7, 0xE7, 0xF7, 0xFF, 0xFF, 0xFF,
0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xE0, 0xE0, 0xE0, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF,
0x00, 0x00, 0xFC, 0xFE, 0xFF, 0xFF, 0xE7, 0xE7, 0xE7, 0xE7, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xE0, 0xE0, 0xE0, 0xE0, 0xF0, 0xFF, 0xFF, 0xFF,
0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xE0, 0xE0,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF8, 0xF9, 0xF9, 0xF9,
0xF9, 0xF8, 0xF8, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9,
0xF8, 0xF8, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xF8,
0xF8, 0xF9, 0x09, 0xF9, 0x09, 0xD8, 0xB8, 0x78, 0x08, 0xF8, 0x09, 0xE9, 0xE9, 0x19, 0xF8, 0x08,
0xF8, 0xF8, 0x08, 0xF8, 0xD8, 0xA8, 0xA9, 0x69, 0xF9, 0xE9, 0x09, 0xE9, 0xF8, 0x09, 0xA9, 0x29,
0xD9, 0xF8, 0x08, 0xF9, 0x09, 0xA9, 0xA9, 0xF8, 0xD8, 0xA8, 0xA9, 0x69, 0xF9, 0xF9, 0xF9, 0xF9,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01,
0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01
};

static  uint8_t displayStartLine=0x40;  // 40...7F   64 ... 127 

static uint8_t  HigherColumnStartAddress=0x10; //10h~1Fh
static uint8_t  LowerColumnStartAddress=0x04; // <5 A F

/// Function prototype for the custom IRQ handler function -- see below for the implementation
static irq_handler_t  ebbgpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);

//----------------------------------------------------------------------------------------------------------

static int init_gpio70()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio70))
   {
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn70 = true;
   gpio_request(gpio70, "sysfs");            // 
   gpio_direction_output(gpio70, ledOn70);   // 
// gpio_set_value(gpio70, ledOn70);          // 
   gpio_export(gpio70, false);               // 
}

static int init_gpio71()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio71))
   {
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn71 = true;
   gpio_request(gpio71, "sysfs");            // gpio71 is hardcoded to 49, request it
   gpio_direction_output(gpio71, ledOn71);   // Set the gpio to be in output mode and on
// gpio_set_value(gpio71, ledOn71);          // Not required as set by line above (here for reference)
   gpio_export(gpio71, false);               // Causes gpio49 to appear in /sys/class/gpio
}

static int init_gpio72()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio72))
   {
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn72 = true;
   gpio_request(gpio72, "sysfs");            // 
   gpio_direction_output(gpio72, ledOn72);   // 
// gpio_set_value(gpio71, ledOn71);          //
   gpio_export(gpio72, false);               //
}

static int init_gpio73()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio73)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn73 = true;
   gpio_request(gpio73, "sysfs");            // 
   gpio_direction_output(gpio73, ledOn73);   // 
// gpio_set_value(gpio71, ledOn71);          //
   gpio_export(gpio73, false);               //
}

static int init_gpio74()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio74)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn74 = true;
   gpio_request(gpio74, "sysfs");            // 
   gpio_direction_output(gpio74, ledOn74);   // 
// gpio_set_value(gpio74, ledOn70);          // 
   gpio_export(gpio74, false);               // 
}

static int init_gpio75()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio75)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn75 = true;
   gpio_request(gpio75, "sysfs");            // 
   gpio_direction_output(gpio75, ledOn75);   // 
// gpio_set_value(gpio75, ledOn70);          // 
   gpio_export(gpio75, false);               // 
}

static int init_gpio76()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio76)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn76 = true;
   gpio_request(gpio76, "sysfs");            // 
   gpio_direction_output(gpio76, ledOn76);   // 
// gpio_set_value(gpio76, ledOn70);          // 
   gpio_export(gpio76, false);               // 
}

static int init_gpio77()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio77)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn77 = true;
   gpio_request(gpio77, "sysfs");            // 
   gpio_direction_output(gpio77, ledOn77);   // 
// gpio_set_value(gpio77, ledOn70);          // 
   gpio_export(gpio77, false);               // 
}

static int init_gpio78()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio78)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn78 = true;
   gpio_request(gpio78, "sysfs");            // 
   gpio_direction_output(gpio78, ledOn78);   // 
// gpio_set_value(gpio78, ledOn70);          // 
   gpio_export(gpio78, false);               // 
}

static int init_gpio79()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio79)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn79 = true;
   gpio_request(gpio79, "sysfs");            // 
   gpio_direction_output(gpio79, ledOn79);   // 
// gpio_set_value(gpio79, ledOn70);          // 
   gpio_export(gpio79, false);               // 
}

static int init_gpio80()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio80)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn80 = true;
   gpio_request(gpio80, "sysfs");            // 
   gpio_direction_output(gpio80, ledOn80);   // 
// gpio_set_value(gpio80, ledOn70);          // 
   gpio_export(gpio80, false);               // 
}

static int init_gpio81()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio81)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn81 = true;
   gpio_request(gpio81, "sysfs");            // 
   gpio_direction_output(gpio81, ledOn81);   // 
// gpio_set_value(gpio81, ledOn70);          // 
   gpio_export(gpio81, false);               // 
}

static int init_gpio8()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio8)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn8 = true;
   gpio_request(gpio8, "sysfs");            // 
   gpio_direction_output(gpio8, ledOn8);    // 
// gpio_set_value(gpio8, ledOn70);          // 
   gpio_export(gpio8, false);               // 
}

static int init_gpio9()
{
   int result = 0;
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(gpio9)){
      printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
      return -ENODEV;
   }
   
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn9 = true;
   gpio_request(gpio9, "sysfs");            // 
   gpio_direction_output(gpio9, ledOn9);    // 
// gpio_set_value(gpio9, ledOn70);          // 
   gpio_export(gpio9, false);               // 
}


//----------------------------------------------------------------------------------------------------------
static void deinit_gpio70()
{
   gpio_set_value(gpio70, 0);                 
   gpio_unexport(gpio70);                  // Unexport the LED GPIO
   gpio_free(gpio70);                      // Free the LED GPIO
}

static void deinit_gpio71()
{
   gpio_set_value(gpio71, 0);                 
   gpio_unexport(gpio71);                  // Unexport the LED GPIO
   gpio_free(gpio71);                      // Free the LED GPIO
}

static void deinit_gpio72()
{
   gpio_set_value(gpio72, 0);                 
   gpio_unexport(gpio72);                  // Unexport the LED GPIO
   gpio_free(gpio72);                      // Free the LED GPIO
}

static void deinit_gpio73()
{
   gpio_set_value(gpio73, 0);                 
   gpio_unexport(gpio73);                  // Unexport the LED GPIO
   gpio_free(gpio73);                      // Free the LED GPIO
}

static void deinit_gpio74()
{
   gpio_set_value(gpio74, 0);                 
   gpio_unexport(gpio74);                  // Unexport the LED GPIO
   gpio_free(gpio74);                      // Free the LED GPIO
}

static void deinit_gpio75()
{
   gpio_set_value(gpio75, 0);                 
   gpio_unexport(gpio75);                  // Unexport the LED GPIO
   gpio_free(gpio75);                      // Free the LED GPIO
}

static void deinit_gpio76()
{
   gpio_set_value(gpio76, 0);                 
   gpio_unexport(gpio76);                  // Unexport the LED GPIO
   gpio_free(gpio76);                      // Free the LED GPIO
}

static void deinit_gpio77()
{
   gpio_set_value(gpio77, 0);                 
   gpio_unexport(gpio77);                  // Unexport the LED GPIO
   gpio_free(gpio77);                      // Free the LED GPIO
}

static void deinit_gpio78()
{
   gpio_set_value(gpio78, 0);                 
   gpio_unexport(gpio78);                  // Unexport the LED GPIO
   gpio_free(gpio78);                      // Free the LED GPIO
}

static void deinit_gpio79()
{
   gpio_set_value(gpio79, 0);                 
   gpio_unexport(gpio79);                  // Unexport the LED GPIO
   gpio_free(gpio79);                      // Free the LED GPIO
}

static void deinit_gpio80()
{
   gpio_set_value(gpio80, 0);                 
   gpio_unexport(gpio80);                  // Unexport the LED GPIO
   gpio_free(gpio80);                      // Free the LED GPIO
}
static void deinit_gpio81()
{
   gpio_set_value(gpio81, 0);                 
   gpio_unexport(gpio81);                  // Unexport the LED GPIO
   gpio_free(gpio81);                      // Free the LED GPIO
}

static void deinit_gpio8()
{
   gpio_set_value(gpio8, 0);                 
   gpio_unexport(gpio8);                  // Unexport the LED GPIO
   gpio_free(gpio8);                      // Free the LED GPIO
}

static void deinit_gpio9()
{
   gpio_set_value(gpio9, 0);                 
   gpio_unexport(gpio9);                  // Unexport the LED GPIO
   gpio_free(gpio9);                      // Free the LED GPIO
}

//-------------------------------------------------------------------------------------------------------

static int init_GPIO(){
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   int result = 0;
   //init out gpio
   result = init_gpio70();
   result = init_gpio71();
   result = init_gpio72();
   result = init_gpio73();
   result = init_gpio74();
   result = init_gpio75();
   result = init_gpio76();
   result = init_gpio77();
   result = init_gpio78();
   result = init_gpio79();
   result = init_gpio80();
   result = init_gpio81();
   result = init_gpio8();
   result = init_gpio9();
}


//-------------------------------------------------------



static void setDataBus(uint8_t data) 
{
  if(data&(1<<0)){
    gpio_set_value( DATA0_PIN, GPIO_PIN_SET);
  }else{
    gpio_set_value( DATA0_PIN, GPIO_PIN_RESET);
  }  
  
  if(data&(1<<1)){
    gpio_set_value( DATA1_PIN, GPIO_PIN_SET);
  }else{
    gpio_set_value( DATA1_PIN, GPIO_PIN_RESET);
  }  
  
  if(data&(1<<2)){
    gpio_set_value( DATA2_PIN, GPIO_PIN_SET);
  }else{
    gpio_set_value( DATA2_PIN, GPIO_PIN_RESET);
  }  
  
  if(data&(1<<3)){
    gpio_set_value( DATA3_PIN, GPIO_PIN_SET);
  }else{
    gpio_set_value( DATA3_PIN, GPIO_PIN_RESET);
  }  
  
  if(data&(1<<4)){
    gpio_set_value( DATA4_PIN, GPIO_PIN_SET);
  }else{
    gpio_set_value( DATA4_PIN, GPIO_PIN_RESET);
  }  
  
  if(data&(1<<5)){
    gpio_set_value( DATA5_PIN, GPIO_PIN_SET);
  }else{
    gpio_set_value( DATA5_PIN, GPIO_PIN_RESET);
  }  
  
  if(data&(1<<6)){
    gpio_set_value( DATA6_PIN, GPIO_PIN_SET);
  }else{
    gpio_set_value( DATA6_PIN, GPIO_PIN_RESET);
  }  
  
  if(data&(1<<7)){
    gpio_set_value( DATA7_PIN, GPIO_PIN_SET);
  }else{
    gpio_set_value( DATA7_PIN, GPIO_PIN_RESET);
  }  
}

static void write_cmd(uint8_t cmmand)
{
  //write
  gpio_set_value( RW_PIN, GPIO_PIN_RESET); // 
  udelay(1);
  
  // command
  gpio_set_value( DC_PIN, GPIO_PIN_RESET);
  udelay(1);
  
  // chip enable
  gpio_set_value( CS_PIN, GPIO_PIN_RESET);
  udelay(1);
  
  setDataBus(cmmand);
  udelay(1); 
  
  gpio_set_value( CS_PIN, GPIO_PIN_SET);
  udelay(1);
  
  gpio_set_value( DC_PIN, GPIO_PIN_SET);
  udelay(1);
  
  gpio_set_value( RW_PIN, GPIO_PIN_SET); 
}

//
//
//
static void write_data(uint8_t data)
{
  //write
  gpio_set_value( RW_PIN, GPIO_PIN_RESET); 
  udelay(1);
  // Data
  gpio_set_value( DC_PIN, GPIO_PIN_SET);
  udelay(1);
  
  // chip enable
  gpio_set_value( CS_PIN, GPIO_PIN_RESET);
  udelay(1);
  
  setDataBus(data);
  udelay(1); 
  
  gpio_set_value( CS_PIN, GPIO_PIN_SET);
  udelay(1);
  
  gpio_set_value( DC_PIN, GPIO_PIN_SET);
  udelay(1);
  
  gpio_set_value( RW_PIN, GPIO_PIN_SET); 
  
}


static void display(void) 
{
  uint16_t i=0;
  uint8_t page;
  uint8_t x;
  uint8_t w;


  for(page = 0; page<8; page++)
  {
    write_cmd(SET_PAGE_START + page);
    write_cmd(LowerColumnStartAddress); //Set Lower Column Start Address for Page Addressing Mode (00h~0Fh)
    write_cmd(HigherColumnStartAddress); //Set Higher Column Start Address for Page Addressing Mode (10h~1Fh)

    // send a bunch of data in one 
    for (w=0; w<128/16; w++) {
      write_cmd(displayStartLine);  //Set Display Start Line (40h~7Fh)
      for ( x=0; x<16; x++) {
        write_data(buffer[i++]);
      }
    }
    
  }
}

static void invertColor(uint8_t i) {
  if (i) {
    write_cmd(SSD1305_INVERTDISPLAY);
  } else {
    write_cmd(SSD1305_NORMALDISPLAY);
  }
}

static void paintUpScreen()
{
  memset(buffer, 0xff, (SSD1305_LCDWIDTH*SSD1305_LCDHEIGHT/8));
  display();
}

static void clearScreen()
{
  memset(buffer, 0, (SSD1305_LCDWIDTH*SSD1305_LCDHEIGHT/8));
  display();
}

static void Display_Init()
{

  msleep(40);                // millisecond sleep for half of the period
  gpio_set_value( RES_PIN, GPIO_PIN_SET);
  msleep(40);
  gpio_set_value( RES_PIN, GPIO_PIN_RESET);
  msleep(40);
  gpio_set_value( RES_PIN, GPIO_PIN_SET);
  msleep(40);
  
  
  gpio_set_value( DISP_PIN, GPIO_PIN_SET);
  msleep(40);
  
  //
  write_cmd(DISPLAY_OFF);//set Off
  write_cmd(SET_SEGMANT_REMAP_SEG0_131);// segment remap
  //write_cmd(0xc4);
  write_cmd(DISPLAY_ON); //включить экранчик
  write_cmd(0x86);//set contrast comand
   
  write_cmd(SET_CONTRAST);
  write_cmd(0xEf);
   

  write_cmd(SET_BRIGHTNESS);
  write_cmd(0x7F);
  
   
  write_cmd(SET_PAGE_START);
   
  display();
  msleep(40);
   
  invertColor(1);
  msleep(40);
  invertColor(0);
  msleep(40);
   
  write_cmd(0x26);//Right Horizontal Scroll
  write_cmd(0x01);
  write_cmd(0x00);
  write_cmd(0x00);
  write_cmd(0x01);
   
  write_cmd(0x27);//Left Horizontal Scroll
  write_cmd(0x01);
  write_cmd(0x00);
  write_cmd(0x00);
  write_cmd(0x01);
   
  paintUpScreen();
   
  clearScreen();
 
}

int16_t width()
{
  return (SSD1305_LCDWIDTH-1);
}

int16_t height()
{
  return (SSD1305_LCDHEIGHT-1);
}

// the most basic function, set a single pixel
static void drawPixel(int16_t col, int16_t line, uint16_t color)
{
  if ((col >= width()) || (line >= height()) || (col < 0) || (line < 0))
    return;
//
//  // check rotation, move pixel around if necessary
//  switch (getRotation()) {
//  case 1:
//    adagfx_swap(x, y);
//    x = WIDTH - x - 1;
//    break;
//  case 2:
//    x = WIDTH - x - 1;
//    y = HEIGHT - y - 1;
//    break;
//  case 3:
//    adagfx_swap(x, y);
//    y = HEIGHT - y - 1;
//    break;
//  }  
//
//  // x is which column
  if (color == WHITE) 
    buffer[col+ (line/8)*SSD1305_LCDWIDTH] |= _BV((line%8));  
  else
    buffer[col+ (line/8)*SSD1305_LCDWIDTH] &= ~_BV((line%8)); 
}



void glcd_pixel(int col, int line, int colour)
{
  drawPixel(col,   line, colour);
}


int bit_test(int x, int y){
  if( x &(1 << y ) )
  {
    return 1;
  }
  else
  {
    return 0;
  }
}


//Fonts (3x6 pixel) 
static const BYTE TEXT_3x6_1[256][4] ={
                0x00,0x00,0x00,0x00, // Space   32  //0
                0x00,0x00,0x5C,0x00, // !             
                0x00,0x0C,0x00,0x0C, // "             2
                0x00,0x7C,0x28,0x7C, // #
                0x00,0x7C,0x44,0x7C, // 0x                         
                0x00,0x24,0x10,0x48, // %
                0x00,0x28,0x54,0x08, // &
                0x00,0x00,0x0C,0x00, // '                          
                0x00,0x38,0x44,0x00, // (                          
                0x00,0x44,0x38,0x00, // )                9                      
                0x00,0x20,0x10,0x08, // //                         
                0x00,0x10,0x38,0x10, // +                          
                0x00,0x80,0x40,0x00, // ,                          
                0x00,0x10,0x10,0x10, // -                          
                0x00,0x00,0x40,0x00, // .                          
                0x00,0x20,0x10,0x08, // /     
                0x00,0x38,0x44,0x38, // 0    0x30                      
                0x00,0x00,0x7C,0x00, // 1                          
                0x00,0x64,0x54,0x48, // 2                          
                0x00,0x44,0x54,0x28, // 3                19          
                0x00,0x1C,0x10,0x7C, // 4                          
                0x00, 0x4C,0x54,0x24, // 5                          
                0x00, 0x38,0x54,0x20, // 6                          
                0x00, 0x04,0x74,0x0C, // 7                          
                0x00, 0x28,0x54,0x28, // 8                          
                0x00, 0x08,0x54,0x38, // 9                          
                0x00, 0x00,0x50,0x00, // :                          
                0x00, 0x80,0x50,0x00, // ;                          
                0x00, 0x10,0x28,0x44, // <                          
                0x00, 0x28,0x28,0x28, // =                   29
                0x00, 0x44,0x28,0x10, // >                         
                0x00, 0x04,0x54,0x08, // ?                          
                0x00, 0x38,0x4C,0x5C, // @    0x40                           
                0x00, 0x78,0x14,0x78, // A                          
                0x00, 0x7C,0x54,0x28, // B                          
                0x00, 0x38,0x44,0x44, // C                          
                0x00, 0x7C,0x44,0x38, // D                          
                0x00, 0x7C,0x54,0x44, // E                          
                0x00, 0x7C,0x14,0x04, // F                          
                0x00, 0x38,0x44,0x34, // G                     39     
                0x00, 0x7C,0x10,0x7C, // H                    
                0x00, 0x00,0x7C,0x00, // I                          
                0x00, 0x20,0x40,0x3C, // J                          
                0x00, 0x7C,0x10,0x6C, // K                          
                0x00, 0x7C,0x40,0x40, // L                          
                0x00, 0x7C,0x08,0x7C, // M                          
                0x00, 0x7C,0x04,0x7C, // N                          
                0x00, 0x7C,0x44,0x7C, // O                          
                0x00, 0x7C,0x14,0x08, // P    0x50                  
                0x00, 0x38,0x44,0x78, // Q                    49      
                0x00, 0x7C,0x14,0x68, // R                          
                0x00, 0x48,0x54,0x24, // S                          
                0x00, 0x04,0x7C,0x04, // T                          
                0x00, 0x7C,0x40,0x7C, // U                          
                0x00, 0x3C,0x40,0x3C, // V                          
                0x00, 0x7C,0x20,0x7C, // W                          
                0x00, 0x6C,0x10,0x6C, // X                          
                0x00, 0x1C,0x60,0x1C, // Y                          
                0x00, 0x64,0x54,0x4C, // Z                          
                0x00, 0x7C,0x44,0x00, // [                    59      
                0x00, 0x08,0x10,0x20, // slesh   sw-se                           
                0x00, 0x44,0x7C,0x00, // ]                          
                0x00, 0x08,0x04,0x08, // ^                          
                0x00, 0x80,0x80,0x80, // _                          
                0x00, 0x04,0x08,0x00, // ` 0x60  96
                0x00, 0x68,0x48,0x78, // a
                0x00, 0x7C,0x48,0x30, // b                  
                0x00, 0x30,0x48,0x48, // c
                0x00, 0x30,0x48,0x7C, // d 100
                0x00, 0x30,0x48,0x58, // e                    69
                0x00, 0x20,0x78,0x28, // f
                0x00, 0x50,0x48,0x38, // g
                0x00, 0x78,0x20,0x60, // h
                0x00, 0x00,0x68,0x00, // i
                0x00, 0x40,0x74,0x00, // j
                0x00, 0x78,0x20,0x50, // k
                0x00, 0x78,0x40,0x00, // l
                0x00, 0x78,0x10,0x78, // m
                0x00, 0x78,0x08,0x70, // n 110
                0x00, 0x30,0x48,0x30, // o                      79
                0x00, 0x78,0x28,0x38, // p
                0x00, 0x10,0x28,0x78, // q
                0x00, 0x78,0x28,0x50, // r
                0x00, 0x28,0x48,0x50, // s
                0x00, 0x10,0x78,0x50, // t
                0x00, 0x78,0x40,0x78, // u
                0x00, 0x38,0x40,0x38, // v
                0x00, 0x78,0x60,0x78, // w
                0x00, 0x68,0x10,0x68, // x 120
                0x00, 0x18,0x60,0x18, // y                         89
                0x00, 0x48,0x68,0x58  // z 122                     90
};


// Purpose:       Write a char on a graphic LCD using the 3x6 bit font
void glcd_putc36(char inputCharacter)
{
// c = [0....255]
  int j, k, l, m;                     // Loop counters
  BYTE pixelData[4];                     // Stores character data
    
  if(inputCharacter == '\r') 
  { //0x0D 
    // colloms
    glcd_x = 0; return; 
  }
    
  if(inputCharacter == '\n') 
  { 
    //row
	//0x0A
    glcd_y += 7 * glcd_size; 
    if(56<glcd_y ){
      glcd_y =0;
    }
    return; 
  }

  if(inputCharacter < 32 || 122 < inputCharacter ) // Checks if the letter is in the first text array
     memcpy(pixelData, TEXT_3x6_1[0], 4);   // Default to space
  else
     memcpy(pixelData, TEXT_3x6_1[inputCharacter-32], 4);
       
  for(j=0; j<4; ++j, glcd_x+=glcd_size) {   // Loop through character byte data
    for(k=0; k<8*glcd_size; ++k) {         // Loop through the vertical pixels
      if(bit_test(pixelData[j], k)) {// Check if the pixel should be set
        for(l=0; l<glcd_size; ++l) {     // The next two loops change the
          for(m=0; m<glcd_size; ++m){
            glcd_pixel(glcd_x+m, glcd_y+k*glcd_size+l, glcd_colour); // Draws the pixel
          }
        }
      }else{// Check if the pixel should be set
        for(l=0; l<glcd_size; ++l) {     // The next two loops change the
          for(m=0; m<glcd_size; ++m){
            glcd_pixel(glcd_x+m, glcd_y+k*glcd_size+l, 0); // Draws the pixel
          }
        }
      }
    }
    if(127<glcd_x){
      glcd_x=0;
    }
  }
  //glcd_x += glcd_size;
  if(127<glcd_x){
    glcd_x=0;
  }
}



//
// Purpose:       Write text string in 3x6 font on a graphic LCD
static void glcd_text36(char* intextptr)
{
  for(; *intextptr != '\0'; ++intextptr)   {               // Loop through the passed string
    glcd_putc36(*intextptr);
    display();
  }
}

//-----------------------------------------------------------------------------------------------------
static int __init lcdDriver_init(void){
   int result = 0;
   printk(KERN_INFO "LCD_TEST: Initialization of display...\n");
   
   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "DISPLAY_DRV failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "DISPLAY_DRV: registered correctly with major number %d\n", majorNumber);

   // Register the device class
   ebbcharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(ebbcharClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(ebbcharClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "DISPLAY_DRV: device class registered correctly\n");

   // Register the device driver
   ebbcharDevice = device_create(ebbcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(ebbcharDevice)){               // Clean up if there is an error
      class_destroy(ebbcharClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(ebbcharDevice);
   }
   printk(KERN_INFO "DISPLAY_DRV: device class created correctly\n"); // Made it! device was initialized   
   
   result = init_GPIO();
   
   Display_Init();

   sprintf(textptr, "\n\rOLED Driver's\n\rBeen\n\rLaunched.");
   
   glcd_text36(textptr);
   
   int k=0;
   for(k=0; k<1024; k++)
   {
      message[k]=0x00;   
   }
   
   
   printk(KERN_INFO "LCD_TEST: Initialization of display has been done.\n");

   return result;
}

//---------------------------------------------------------------------------------------------------
static void __exit lcdDriver_exit(void){
   //char textptr[300]="\n\rGood buy!";
   printk(KERN_INFO "DISPLAY_DRV: Start deinit.\n");
   
   sprintf(textptr, "\n\rGoodbye!");
   
   printk(KERN_INFO "DISPLAY_DRV: Start printing.\n");
   glcd_text36(textptr);
   
   deinit_gpio70();
   deinit_gpio71();
   deinit_gpio72();
   deinit_gpio73();
   deinit_gpio74();
   deinit_gpio75();
   deinit_gpio76();
   deinit_gpio77();
   deinit_gpio78();
   deinit_gpio79();
   deinit_gpio80();
   deinit_gpio81();
   deinit_gpio8();
   deinit_gpio9();
   
   
   device_destroy(ebbcharClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(ebbcharClass);                          // unregister the device class
   class_destroy(ebbcharClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
 
   printk(KERN_INFO "DISPLAY_DRV: Goodbye from the LKM!\n");
}


/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   printk(KERN_INFO "DISPLAY_DRV: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
 //  int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
 //  error_count = copy_to_user(buffer, message, size_of_message);

 //  if (error_count==0){            // if true then have success
 //     printk(KERN_INFO "LCD_TEST: Sent %d characters to the user\n", size_of_message);
 //     return (size_of_message=0);  // clear the position to the start and return 0
//   }
//   else {
//      printk(KERN_INFO "LCD_TEST: Failed to send %d characters to the user\n", error_count);
//      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
//   }
   
   printk(KERN_INFO "LCD_TEST: Read\n");
   
   int mylen = strlen( info_str );
   if( len < mylen ) return -EINVAL;
   if( *offset != 0 ) {
		return 0;
	}
	if( copy_to_user( buffer, info_str, mylen ) ) return -EINVAL;
	*offset = mylen;
	return mylen;
   
   
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
   sprintf(&message[0], "%s", buffer, len);   // appending received string with its length
   size_of_message = strlen(message);         // store the length of the stored message does not work
   printk(KERN_INFO "LCD_TEST: size_of_message: %d \n", size_of_message);
   printk(KERN_INFO "LCD_TEST: len: %d \n", len);
   
   if( (0<len) && (len<200)  )
   {  
      printk(KERN_INFO "LCD_TEST: print message to screen\n", len);
      sprintf(textptr, message); 
       //sprintf(textptr, "%s (%d let)", buffer, len);   // appending received string with its length
      textptr[len+1] = '\0';
      glcd_text36(textptr);
   }
   
//   int i;
//   for(i=0;i<len; i++)
//   {
//	   glcd_putc36(buffer[i]);
//	   display();
   //}
   
   printk(KERN_INFO "LCD_TEST: Received %d characters from the user\n", len);
   return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "LCD_TEST: Device successfully closed\n");
   return 0;
}

/// This next calls are  mandatory -- they identify the initialization function
/// and the cleanup function (as above).
module_init(lcdDriver_init);
module_exit(lcdDriver_exit);
