#include "io430.h"
#include "ht1621.h"
#include "in430.h"

#define DATA   0        //天线口
#define CHANGE 1      //切换口
#define WAKEUP 2      //唤醒口
#define KEY    3        //授时口
#define SOLAR  4        //太阳电池口

#define SECONDCLOCK 0 //秒表口 p2端口

#define PIN P1IN      
#define PDIR P1DIR
#define PIE  P1IE
#define PIES P1IES
#define PIFG P1IFG
#define POUT P1OUT
#define PREN P1REN

#define CALBC1_1MHZ_          (0x10FFu)
#define CALDCO_1MHZ_          (0x10FEu)
#define LFXT1S_2            (0x20)

unsigned int serial[40];   //接收结果
unsigned int pointer=0;    
unsigned int breakpoint=0; 
unsigned int flag=0;

unsigned int dayofweek=0; //星期
unsigned int day=0;       //日
unsigned int month=0;     //月

unsigned int s;            //秒
unsigned int minute;       //分
unsigned int hour;         //小时

unsigned int changeCalendar=0; //与change配合使用
unsigned int calendarShow=0;   //防止1s中断对显示日期的干扰

unsigned int sleepCount=0;    //控制休眠
unsigned int state=0;         //单片机状态 0正常 1 休眠

unsigned int solarSleepFlag=0;

unsigned int secondtime=0;   //秒表计数


void init_timer(){
     TA1CCTL0&=~(1<<4); //清楚time1中断；
     TACTL|=1<<8;  //时钟32khz
     TACTL|=1<<5;  //计数到0xFFFF
     
     PDIR&=~(1<<DATA);  //输入
     PIE|=(1<<DATA);    //使能中断
     PIFG=0;            
     PIES&=~(1<<DATA);  //上升沿触发
     PIFG=0;            //清除中断请求
}

#pragma vector=PORT1_VECTOR
__interrupt void TA_ISR(void){
  if(PIFG & (1<<DATA)) {
    if((PIES & (1<<DATA))==0){
       if(pointer>0){
          float t=TAR;
          TAR=0x0000;
          float second=(t*32)/1000000;
          if(second>1.4)
            breakpoint=pointer;
       }
       if(pointer==0){
          TAR=0x0000;
       }
       PIES|=1<<DATA;
     }else if(PIES & (1<<DATA)){
         float t=TAR;
         TAR=0x000;
         float second=(t*32)/1000000;
         if((0.05<=second) && (second<0.20)){
            serial[pointer]=0;
            pointer++;
            time[1]=0;
            time[2]=0;            
            time[0]=pointer;
            show_time();
	 }
         else if((0.20<=second)&&(second<0.30)){
            serial[pointer]=1;
	    pointer++;
            time[1]=0;
            time[2]=0;            
            time[0]=pointer;
            show_time();
	 }
         else if((0.30<=second)&&(second<0.43)){
            serial[pointer]=2;
            pointer++;
            time[1]=0;
            time[2]=0;            
            time[0]=pointer;
            show_time();
         }
         else if(0.43<=second){
            serial[pointer]=3;
            pointer++;
            time[1]=0;
            time[2]=0;            
            time[0]=pointer;
            show_time();
         }
         if(pointer==19){
            flag=1;
         }
         PIES&=~(1<<DATA);
     }
     PIFG&=~(1<<DATA);
  }
  if(PIFG & (1<<KEY)) {
     int i;
     for(i=0;i<2000;i++){};
     if((PIN & (1<<KEY))==0){
        init_timer();
        sleepCount=0;
     }
     PIFG&=~(1<<KEY);
  }
  if(PIFG & (1<<CHANGE)) {
     int i;
     for(i=0;i<2000;i++){};
     if((PIN & (1<<CHANGE))==0){
       if(state==0){
         sleepCount=0;
         if(changeCalendar==1){
          time[2]=hour;
          time[1]=minute;
          time[0]=s;
          show_time();
          changeCalendar=0;
          calendarShow=0;  
        }else if(changeCalendar==0){
          time[2]=month;
          time[1]=day;
          time[0]=dayofweek;
          show_time();
          changeCalendar=1;
          calendarShow=1;
        }
       }else if(state==2){
          if(secondtime>0)
            secondtime--;
          time[0]=secondtime;
          time[1]=0;
          time[2]=0;
          show_time();
       }
     }
     PIFG&=~(1<<CHANGE);
  }   
  if(PIFG & (1<<WAKEUP)) {
     int i;
     for(i=0;i<2000;i++){};
     if((PIN & (1<<WAKEUP))==0){
       if(state==1){
          sleepCount=0;
          state=0;
          _BIC_SR_IRQ(0x0010u+0x0040u+0x0080u);
       }
       else if(state==2){
          secondtime++;
          time[0]=secondtime;
          time[1]=0;
          time[2]=0;
          show_time();
       }
     }
     PIFG&=~(1<<WAKEUP);
  }    
}

#pragma vector=PORT2_VECTOR
__interrupt void TA_ISR2(void){
     if(P2IFG & (1<<SECONDCLOCK)) {
        int i;
        for(i=0;i<1000;i++){};
        if((P2IN & (1<<SECONDCLOCK))==0){
          if(state==0){
              sleepCount=0;
              state=2;
              time[0]=0;
              time[1]=0;
              time[2]=0;
              secondtime=0;
              show_time();
          }
        }else if(state==2){
            state=3;
            sleepCount=0;
        }
        else if(state==3){
            state=0;
            time[2]=0;
            time[1]=0;
            time[0]=0;
            show_time();
            init_timer();
            sleepCount=0;            
        }
     P2IFG&=~(1<<SECONDCLOCK);
  }    
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void TA1_ISR(void){
    s++;
    if(s>=60){
      s=0;
      minute++;
      if(minute>=60){
        minute=0;
        if(hour>=12){
          hour=0;
        }
      }
    }
    
    //控制休眠
    
    if(state==1 && solarSleepFlag==1 && (PIN & (1<<SOLAR))){
        sleepCount=0;
        state=0;
        solarSleepFlag=0;
        _BIC_SR_IRQ(0x0010u+0x0040u+0x0080u);
    }
    
    if(state==0 && ((PIN & (1<<SOLAR))==0)){
        ht1621ClearData();
        state=1;
        solarSleepFlag=1;
    }
    
    sleepCount++;
    if(sleepCount==12 && state==0){
      ht1621ClearData();
      state=1;
    }
    
    if(calendarShow==0 && state==0){
      time[2]=hour;
      time[1]=minute;
      time[0]=s;
      show_time();
    }
    if(state==3){
      sleepCount=0;
      if(secondtime>0)
        secondtime--;
      time[0]=secondtime;
      time[1]=0;
      time[2]=0;
      show_time();
    }
    TA1CCTL0&=~(1<<0);    //清除中断标志
}

void clock_Init(void)
{
    BCSCTL1 = CALBC1_1MHZ; 		           // Set range
    DCOCTL = CALDCO_1MHZ;  		           // Set DCO step + modulation

    BCSCTL3 |= LFXT1S_0+XCAP_3;              // LFXT1 = 32768khz外部晶振,此时ACLK=32768khz

    while(IFG1 & OFIFG)
    {
      IFG1 &= ~OFIFG;                         // 清除 OSCFault 标志
      __delay_cycles(600000);                  // 为可见的标志延时
    }
    IFG1 &= ~OFIFG;                          // Clear OSCFault flag
    BCSCTL2 |= SELM_0 + DIVM_0;              // MCLK = DCO/1
}

void init_timer1(){
     PIE&=~(1<<DATA);  //禁止time0的中断
  
     TA1CTL|=1<<8; //时钟32khz
     TA1CTL|=1<<4; //计数到TA1CCR0
     
     TA1CCR0=31250; //匹配时为一秒
     TA1CCTL0|=1<<4; //使能中断；
     TA1R=0;
}

void init_key(){
     PDIR&=~(1<<KEY);
     PIES|=1<<KEY;
     PIE|=1<<KEY;
     PREN|=1<<KEY;
     POUT|=1<<KEY;
}

void init_change(){
     PDIR&=~(1<<CHANGE);
     PIES|=1<<CHANGE;
     PIE|=1<<CHANGE;
     PREN|=1<<CHANGE;
     POUT|=1<<CHANGE;
}

void init_wakeup(){
     PDIR&=~(1<<WAKEUP);
     PIES|=1<<WAKEUP;
     PIE|=1<<WAKEUP;
     PREN|=1<<WAKEUP;
     POUT|=1<<WAKEUP;
}

void init_solar(){
     PDIR&=~(1<<SOLAR);
}

void init_secondclock(){
     P2DIR&=~(1<<SECONDCLOCK);
     P2IES|=1<<SECONDCLOCK;
     P2IE|=1<<SECONDCLOCK;
     P2REN|=1<<SECONDCLOCK;
     P2OUT|=1<<SECONDCLOCK;
}

void main(){
     WDTCTL = WDTPW + WDTHOLD;
     clock_Init(); 
     init_timer();
     init_ht1621();
     show_time();
     init_key();
     init_change();
     init_wakeup();
     init_solar();
     init_secondclock();
     _EINT();

     while(1){
       if(flag==1){
          flag=0;
          _DINT();
          int startpoint=breakpoint;
          int k=breakpoint-1;
          int j=19;
          int i;
          for(i=0;i<=k;i++){
            serial[j]=serial[i];
            i++;
            j++;
          }
          pointer=0;
          breakpoint=0;
          
          hour=serial[startpoint+2]*4+serial[startpoint+3];
          if(serial[startpoint]==0)
            s=1;
          else if(serial[startpoint]==1)
            s=21;
          else if(serial[startpoint]==2)
            s=41;
          s=s+20-startpoint;
          minute=serial[startpoint+4]*16+serial[startpoint+5]*4
            +serial[startpoint+6];
          dayofweek=serial[startpoint+7]*4+serial[startpoint+8];                         //星期
          day=serial[startpoint+10]*16+serial[startpoint+11]*4+serial[startpoint+12];    //日
          month=serial[startpoint+13]*4+serial[startpoint+14];                           //月
          
          dayofweek=3;
          day=19;
          month=9;
          
          time[2]=hour;
          time[1]=minute;
          time[0]=s;
          show_time();
          
          init_timer1();   //开始每秒中断一次
          
          _EINT();
          PIFG=0;
       }
       if(state==1){
          __low_power_mode_3();
       }
    }
}