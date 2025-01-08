#include <computer.h>

void init() //初始化
{
	light = 1;
	buzzer = 1;
	lcden = 1; //使能信号为高电平
	rw = 0;
	clear_numoper();

	i=0;
	j=0;
	temp=0;
	temp_num=0;

	shift_compute_flag=0;
	shift_mode=0;

	conti_light_time=0;
	conti_light=0;
	write_com(0x38); // 8位，2行
	delay(5);
	write_com(0x38); // 8位，2行
	delay(5);
	write_com(0x0c); //显示开，光标关，不闪烁*/
	delay(1);
	write_com(0x06); //增量方式不移位
	delay(1);
	write_com(0x80); //检测忙信号
	delay(1);
	write_com(0x01); //清屏
 	timer0_init();      // 初始化定时器0
    key_interrupt_init();  // 初始化按键中断
}


//
void timer0_init() {
    TMOD |= 0x01;  // 设置定时器0为模式1
    TH0 = 0x00;    // 设置定时器初值
    TL0 = 0x00;
    ET0 = 1;       // 使能定时器0中断
    EA = 1;        // 使能全局中断
    TR0 = 1;       // 启动定时器
}

// 按键中断初始化
void key_interrupt_init() {
    IT0 = 1;    // 设置外部中断0为边缘触发
    EX0 = 1;    // 使能外部中断0
    EA = 1;     // 使能全局中断
}
 
// 外部中断0服务程序：按键按下时重置计时器
// void key_ISR(void) interrupt 0 {
//     idle_time = 0;  // 重置空闲时间计数器
// }

// 中断服务程序：定时器0
void Timer0_Rountine(void) interrupt 1 {
    TH0 = 0x00;    // 定时器初值
    TL0 = 0x00;
    
    idle_time++;  // 空闲时间增加
    conti_light_time++;

	if(conti_light_time>=conti_light_time_threshold&&conti_light!=0){
		conti_light_time=0;
		light_led(100);
	}

    // 如果空闲时间超过设定的阈值，触发关机操作
    if (idle_time >= timeout_threshold && power_on != 0 && standby!=0) {
        shutdown_device();
    }

	
}


void delay(uchar z) // 延迟函数
{
	uchar y;
	for (z; z > 0; z--)
		for (y = 0; y < 110; y++)
			;
}

void write_com(uchar com) // 写指令函数
{
	rs = 0;
	P0 = com; // com指令付给P0口
	delay(5);
	lcden = 1;
	delay(5);
	lcden = 0;
}

void write_date(uchar date) // 写数据函数
{
	rs = 1;
	P0 = date;
	delay(5);
	lcden = 1;
	delay(5);
	lcden = 0;
}
void voice(uchar voice_time)
{
	//高位有声音
	buzzer = 0;
	delay(voice_time);
	buzzer = 1;
	delay(voice_time);
}
void light_led(uchar light_time)
{
	light = 0;
	delay(light_time);
	light = 1;
	delay(light_time);
}
void clean_LCD(){
	write_com(0x01);
	if(power_on!=0)
		write_com(0x0f);
	else
		write_com(0x0c);
}
void shutdown_device() {
	power_on=0;
	idle_time=0;
	delay(5);
	clean_LCD();
	clear_numoper();
	light_led(1000);
	voice(1000);
	light_led(1000);
	voice(1000);
	light_led(1000);
	voice(1000);
}

void control_power(){
	if(button == 0){
		voice(1000);
		light_led(1000);
		delay(100);		    //延时消抖
		while(button==0);	//松手检测
		delay(100);		    //延时消抖
		power_on=~power_on;
		idle_time=0;
		standby=1;
		delay(5);
		clean_LCD();
		if(legal_result==0){
			//控制led恢复
			light=1;
		}
	}
}

void after_key(){
	if(num_number == 0 && shift_mode!=0){
		i = table2[temp_num];	  //数据显示做准备
		write_date(0x30 + i); //显示数据或操作符号
	}else{
		i = table1[temp_num];	  //数据显示做准备
		write_date(0x30 + i); //显示数据或操作符号
	}
	voice(50);
	//前三层按键按下后都不允许standy了
	standby = 0;
	if(legal_result==0){
		//控制led恢复
		light=1;
	}
}

void compute_unlegal(){
	light=0;
	voice(200);
	voice(200);
	voice(200);
	legal_result=0;
}

main()
{
	init(); //系统初始化
	while (1)
	{
		keyscan_new();
	}
}
