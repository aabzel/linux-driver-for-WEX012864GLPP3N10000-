# linux-driver-for-WEX012864GLPP3N10000-
The linux driver for graphic monochrome OLED display.  The WEX012864GLPP3N10000 is based on SSD1305 controller.

List of required hardware:
1.	Beagle Bone Black board
2.	WEX012864GLPP3N10000 display


The rule of the connection the display to BBB board.


______________pinout____________________                            
|CON1#|	Symbol|  BBB HEAD PIN |	GPIO#  |                 
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
                                                    

In order to use GPIO pins for LCD it is necessary to disable the HDMI Cape.

Instruction:

1. Open boot/uEnv.txt

2. To disable the HDMI Cape, change the contents of uEnv.txt to:
Add line: 

optargs=quiet drm.debug=7 capemgr.disable_partno=BB-BONELT-HDMI,BB-BONELT-HDMIN

3. save boot/uEnv.txt

4. Reboot the board:  shutdown -r now




 

