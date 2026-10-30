// SPDX-License-Identifier: MIT
// D1 USB HID event diagnostic. Reads keyboard/mouse evdev for a bounded period.
// Does not grab or modify devices, change display modes, or write to HDMI.
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static const char *paths[] = {
 "/dev/input/by-id/usb-SEMICO_USB_Keyboard-event-kbd",
 "/dev/input/by-id/usb-SIGMACHIP_Usb_Mouse-event-mouse"
};
int main(void) {
 int fd[2] = {-1,-1};
 struct pollfd p[2];
 for(int i=0;i<2;i++) {
   fd[i]=open(paths[i],O_RDONLY|O_NONBLOCK|O_CLOEXEC);
   if(fd[i]<0) {
     fprintf(stderr,"HID_OPEN_FAIL %s: %s\n",paths[i],strerror(errno));
     for(int j=0;j<i;j++)close(fd[j]);
     return 1;
   }
   p[i]=(struct pollfd){.fd=fd[i],.events=POLLIN};
   printf("HID_OPEN_PASS %s\n",paths[i]);
 }
 puts("Press WASD, arrows, mouse buttons and move mouse for 15 seconds.");
 fflush(stdout);
 struct timespec start,now;
 clock_gettime(CLOCK_MONOTONIC,&start);
 while(1) {
   clock_gettime(CLOCK_MONOTONIC,&now);
   long elapsed=(now.tv_sec-start.tv_sec)*1000L+(now.tv_nsec-start.tv_nsec)/1000000L;
   if(elapsed>=15000)break;
   int n=poll(p,2,250);
   if(n<0){if(errno==EINTR)continue;perror("poll");break;}
   for(int i=0;i<2;i++) {
      if(!(p[i].revents&POLLIN))continue;
      struct input_event ev;
      while(read(fd[i],&ev,sizeof(ev))==(ssize_t)sizeof(ev)) {
         if(ev.type==EV_KEY || ev.type==EV_REL)
           printf("%s %s code=%u value=%d\n",
            i==0?"KEYBOARD":"MOUSE",ev.type==EV_KEY?"KEY":"REL",ev.code,ev.value);
      }
   }
 }
 for(int i=0;i<2;i++)close(fd[i]);
 puts("D1_HID_DIAG_COMPLETE (read-only, no grab)");
 return 0;
}
