# boot
Simple bootloader

to create image of bootloader run  
* $ make image  
image might be run by  
* $ qemu-system-i386 -fda image  
  
to install boot on device run  
* $ make install  
this will create image and run tools/install utility  
  
for more info run 
* $ make  
or  
* $ make usage  
