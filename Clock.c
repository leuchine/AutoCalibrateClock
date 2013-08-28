#include "io430.h"
#include "ht1621.h"
#include "in430.h"

#define DATA   0        //���߿�
#define CHANGE 1      //�л���
#define WAKEUP 2      //���ѿ�
#define KEY    3        //��ʱ��
#define SOLAR  4        //̫����ؿ�

#define SECONDCLOCK 0 //���� p2�˿�

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

unsigned int serial[40];   //���ս��
unsigned int pointer=0;    
unsigned int breakpoint=0; 
unsigned int flag=0;

unsigned int dayofweek=0; //����
unsigned int day=0;       //��
unsigned int month=0;     //��

unsigned int s;            //��
unsigned int minute;       //��
unsigned int hour;         //Сʱ

unsigned int changeCalendar=0; //��change���ʹ��
unsigned int calendarShow=0;   //��ֹ1s�ж϶���ʾ���ڵĸ���

unsigned int sleepCount=0;    //��������
unsigned int state=0;         //��Ƭ��״̬ 0���� 1 ����

unsigned int solarSleepFlag=0;

unsigned int secondtime=0;   //������


void init_timer(){
     TA1CCTL0&=~(1<<4); //���time1�жϣ�
     TACTL|=1<<8;  //ʱ��32khz
     TACTL|=1<<5;  //������0xFFFF
     
     PDIR&=~(1<<DATA);  //����
     PIE|=(1<<DATA);    //ʹ���ж�
     PIFG=0;            
     PIES&=~(1<<DATA);  //�����ش���
     PIFG=0;            //����ж�����
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
    
    //��������
    
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
    TA1CCTL0&=~(1<<0);    //����жϱ�־
}

void clock_Init(void)
{
    BCSCTL1 = CALBC1_1MHZ; 		           // Set range
    DCOCTL = CALDCO_1MHZ;  		           // Set DCO step + modulation

    BCSCTL3 |= LFXT1S_0+XCAP_3;              // LFXT1 = 32768khz�ⲿ����,��ʱACLK=32768khz

    while(IFG1 & OFIFG)
    {
      IFG1 &= ~OFIFG;                         // ��� OSCFault ��־
      __delay_cycles(600000);                  // Ϊ�ɼ��ı�־��ʱ
    }
    IFG1 &= ~OFIFG;                          // Clear OSCFault flag
    BCSCTL2 |= SELM_0 + DIVM_0;              // MCLK = DCO/1
}

void init_timer1(){
     PIE&=~(1<<DATA);  //��ֹtime0���ж�
  
     TA1CTL|=1<<8; //ʱ��32khz
     TA1CTL|=1<<4; //������TA1CCR0
     
     TA1CCR0=31250; //ƥ��ʱΪһ��
     TA1CCTL0|=1<<4; //ʹ���жϣ�
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
          dayofweek=serial[startpoint+7]*4+serial[startpoint+8];                         //����
          day=serial[startpoint+10]*16+serial[startpoint+11]*4+serial[startpoint+12];    //��
          month=serial[startpoint+13]*4+serial[startpoint+14];                           //��
          
          dayofweek=3;
          day=19;
          month=9;
          
          time[2]=hour;
          time[1]=minute;
          time[0]=s;
          show_time();
          
          init_timer1();   //��ʼÿ���ж�һ��
          
          _EINT();
          PIFG=0;
       }
       if(state==1){
          __low_power_mode_3();
       }
    }
}