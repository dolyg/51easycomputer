#include <reg51.h>
#define uint unsigned int
#define uchar unsigned char

sbit lcden = P1 ^ 1; // LCD1602控制引脚,使能引脚E
sbit rs = P1 ^ 0;
sbit rw = P1 ^ 2;
sbit busy = P0 ^ 7;
sbit buzzer = P1 ^ 3;
sbit light = P1 ^ 4;
sbit button = P1 ^ 5;
sbit shift_button=P1 ^ 7;

uint power_on= 0;
uchar mode = 0; //记录清零键状态
uchar standby=0;//开机后是否有输入
uchar legal_result=1;//代表是否进入结果非法状态，如果为0，发生其余按键都要关灯

char shift_compute_flag;	//标志运算的时候用十六进制还是正常
char shift_mode;			//标志按下了shift_mode
int hex_result;

//
uchar conti_light;
uchar conti_light_time;
uchar conti_light_time_threshold=20;	//一秒闪烁

//
int num[6]; 		//操作数
char oper[6];		//运算符
int num_number;     //操作数的数目
int oper_number;	//运算符的数目
int num_number_threshold = 5;
int oper_number_threshold = 4;

double result;			//结果
int temp_result;
float temp_result_float;
int i,j,temp,temp_num;

// //栈操作
float num_stack[6];  // 数字栈，用于存储操作数
char oper_stack[6];   // 运算符栈，用于存储运算符
char two_point;
int num_top;       // 数字栈顶指针
int oper_top;      // 运算符栈顶指针

char op;
float num1;
float num2;
char legal_flag;
int digits;
uint digit_threshold = 4;
uint result_threshold = 32767;
unsigned int idle_time = 0;  // 空闲时间计数器
unsigned int timeout_threshold = 170;  // 设定的超时阈值，单位：ms 设定为400大概为30s


uchar code table[] = { //运算数字输入数组
	7, 8, 9, 0,
	4, 5, 6, 0,
	1, 2, 3, 0,
	0, 0, 0, 0};

uchar code table1[] = {
	//经处理后进行键输入显示准备的数组 lcd
	7, 8, 9, 0x2f - 0x30,					 // 7，8，9，÷
	4, 5, 6, 0x2a - 0x30,					 // 4, 5, 6，×
	1, 2, 3, 0x2d - 0x30,					 // 1, 2, 3，-
	0x01 - 0x30, 0, 0x3d - 0x30, 0x2b - 0x30 // C，0，=，+
};
uchar code table2[] = {
	//hex显示
	7, 8, 9, 0x2f - 0x30,					 // 7，8，9，÷
	0x15, 0x16, 6, 0x2a - 0x30,					 // 4, 5, 6，×
	0x12, 0x13, 0x14, 0x2d - 0x30,					 // 1, 2, 3，-
	0x01 - 0x30, 0x11, 0x3d - 0x30, 0x2b - 0x30 // C，0，=，+
};

void timer0_init();

// 按键中断初始化
void key_interrupt_init();
 
// // 外部中断0服务程序：按键按下时重置计时器
// void key_ISR(void) ;
char compute_legal();
void compute();
void delay(uchar z) ;
void after_key();
void write_com(uchar com);

void write_date(uchar date);
void voice(uchar voice_time);
void light_led(uchar light_time);
void clean_LCD();
void shutdown_device() ;
void compute_unlegal();
// // 中断服务程序：定时器0
// void timer0_ISR(void) ;

void init() ;

void control_power();

void keyscan() ;

void control_shift(){
	if(shift_button == 0){
		delay(100);		    //延时消抖
		while(shift_button==0);	//松手检测
		delay(100);		    //延时消抖
		shift_mode=~shift_mode;
		delay(5);
	}
}

void clean_stack(){
	for(i=0;i<6;i++){
		num_stack[i]=0;
		oper_stack[i]=0;
	}
	num_top = -1;  
	oper_top = -1; 
	temp_result_float=0;
}

void clear_numoper(){
	for(i=0;i<6;i++)
		num[i]=0;
	for(i=0;i<6;i++)
		oper[i]=0;	
	hex_result=0;
	shift_compute_flag=0;
	num_number=0;
	oper_number=0;
	result=0;
}

// 比较运算符的优先级，用于确定计算顺序
int comparePriority(char op) {
    if (op == '*' || op == '/') {
        return 2;
    } else if (op == '+' || op == '-') {
        return 1;
    }
    return 0;
}
int abs(int num) {
    if (num < 0) {
        return -num;
    }
    return num;
}
int countDigits(int num) {
    if (num == 0) {
        return 1;
    }
    digits = 0;
    num = abs(num);  // 取绝对值，处理负数情况
    while (num > 0) {
        num /= 10;
        digits++;
    }
    return digits;
}

char hex_map(uchar input_num){
	switch (input_num) {
		case 0:
			return 10;
			break;
		case 1:
			return 11;
			break;
		case 2:
			return 12;
			break;
		case 3:
			return 13;
			break;
		case 4:
			return 14;
			break;
		case 5:
			return 15;
			break;
		default:
			return input_num;
			break;
	}
}

// 执行具体的运算操作
float calculate(float num1, char op, float num2) {
    switch (op) {
    case '+':
        return num1 + num2;
    case '-':
        return num1 - num2;
    case '*':
        return num1 * num2;
    case '/':
        return num1 / num2;
    default:
        return 0;
    }
}

// 解析并计算表达式的主函数
void evaluateExpression() {
	clean_stack();
    for (i=0; i <= num_number; i++) {
        num_stack[++num_top] = num[i];  // 将操作数压入数字栈
        if (i < oper_number) {
            // 当运算符栈非空，且当前运算符优先级小于等于栈顶运算符优先级时，进行计算
            while (oper_top >= 0 && comparePriority(oper_stack[oper_top]) >= comparePriority(oper[i])) {
                num2 = num_stack[num_top--];
                num1 = num_stack[num_top--];
                op = oper_stack[oper_top--];
                temp_result_float = calculate(num1, op, num2);
                num_stack[++num_top] = temp_result_float;
            }
            oper_stack[++oper_top] = oper[i];  // 将当前运算符压入运算符栈
        }
    }

    // 处理剩余的运算符
    while (oper_top >= 0) {
        num2 = num_stack[num_top--];
        num1 = num_stack[num_top--];
        op = oper_stack[oper_top--];
        temp_result_float = calculate(num1, op, num2);
        num_stack[++num_top] = temp_result_float;
    }

    result = num_stack[num_top];
}

void compute_and_display(){
	write_com(0x0c); //光标消失
	write_com(0x80 + 0x4f); //按下等于键，光标前进至第二行最后一个显示处
	write_com(0x04);		//设置从后住前写数据，每写完一个数据，光标后退一格
	legal_flag=1;
	//合理性计算
	if(num_number!=oper_number)
		legal_flag=0;
	for(i=0;i<=num_number;++i){
		if(countDigits(num[i])>digit_threshold)
			legal_flag=0;
	}
	if(legal_flag!=1){
		//非法处理
		compute_unlegal();
		return ;
	}

	//compute
	two_point=0;
	for(i= 0;i<=5;i++)
		if(oper[i]=='/')
			two_point=1;
	evaluateExpression();	
	//结果合法性计算
	if(result>result_threshold){
		compute_unlegal();
		return ;
	}
	//display
	temp_result=0;
	i=0;
	if(two_point==0){
		temp_result=(int)result;
		while ( temp_result!= 0)
		{
			write_date(0x30 + temp_result % 10);
			temp_result = temp_result / 10;
		}
		if(result<0)
			write_date(0x2d);
		if (result==0)
			write_date(0x30 + 0);
	}else{
		//除法
		temp_result=(int)(result*100);
		while (temp_result != 0)
		{
			write_date(0x30 + temp_result % 10);
			temp_result = temp_result / 10;
			i++;
			if (i == 2)	//小数点
				write_date(0x2e);
		}
		if ((int)result == 0)
		{//补上前导0
			write_date(0x30); 
		}
		if(result<0)
			write_date(0x2d);
	}

	write_date(0x3d); //再写"="
	clear_numoper();
}


void keyscan_new(){
	control_power();
	if(power_on==0){
		return ;
	}
	//检测shift

	control_shift();

	P2 = 0xfe; // 1111 1110  第一行为低电平
	if (P2 != 0xfe)
	{
		delay(20); // 延迟20ms
		if (P2 != 0xfe)
		{					  // 1101 1110 & 1111 0000
			temp = P2 & 0xf0; //屏蔽低四位，只考虑高四位
			switch (temp)
			{ // 0xe0=1110 0000，对应着 7 键位
			case 0xe0:
				temp_num = 0;
				break; // 7
			case 0xd0:
				temp_num = 1;
				break; // 8
			case 0xb0:
				temp_num = 2;
				break; // 9
			case 0x70:
				temp_num = 3;
				break; //÷
			}
		}
		while (P2 != 0xfe)
			;								  // 1111 1110
		if (temp_num == 0 || temp_num == 1 || temp_num == 2) //如果按下的是'7','8'或'9'
		{
			if (j != 0)
			{
				write_com(0x01);
				clear_numoper();
				conti_light=0;
				j = 0;
			}
			num[num_number]= num[num_number]*10+table[temp_num];
			//如果没有输入计算符号
			if(num_number==0){
				if(shift_mode!=0){ 
					//如果是shift模式
					shift_compute_flag=1;
					hex_result=hex_result*16+hex_map(table[temp_num]);
				}
				else
					hex_result=hex_result*16+table[temp_num];
			}

		}
		else //如果按下的是'/'  	  除法
		{
			if(num_number!=6)
				num_number++;
			oper[oper_number++]= '/';
		}
		
		after_key();
	}//if (P2 != 0xfe)

	P2 = 0xfd; // 1111 1101
	if (P2 != 0xfd)
	{
		delay(20);
		if (P2 != 0xfd)
		{
			temp = P2 & 0xf0;
			switch (temp)
			{
			case 0xe0:
				temp_num = 4;
				break; // 4
			case 0xd0:
				temp_num = 5;
				break; // 5
			case 0xb0:
				temp_num = 6;
				break; // 6
			case 0x70:
				temp_num = 7;
				break; //×
			}
		}
		while (P2 != 0xfd)
			;								  //等待按键释放
		if (temp_num == 4 || temp_num == 5 || temp_num == 6) //如果按下的是'4','5'或'6'
		{
			if (j != 0)
			{
				write_com(0x01);
				clear_numoper();
				conti_light=0;
				j = 0;
			}
			num[num_number]= num[num_number]*10+table[temp_num];

			//如果没有输入计算符号
			if(num_number==0){
				if(shift_mode!=0){ 
					//如果是shift模式
					shift_compute_flag=1;
					hex_result=hex_result*16+hex_map(table[temp_num]);
				}
				else
					hex_result=hex_result*16+table[temp_num];
			}
		}
		else //如果按下的是'×'
		{
			if(num_number!=6)
				num_number++;
			oper[oper_number++]= '*';	
		}
		// i = table1[num];	  //数据显示做准备
		// write_date(0x30 + i); //显示数据或操作符号
		// voice(50);
		after_key();
	}//if (P2 != 0xfd)

	P2 = 0xfb; // 1111 1011
	if (P2 != 0xfb)
	{
		delay(20);
		if (P2 != 0xfb)
		{
			temp = P2 & 0xf0;
			switch (temp)
			{
			case 0xe0:
				temp_num = 8;
				break; // 1
			case 0xd0:
				temp_num = 9;
				break; // 2
			case 0xb0:
				temp_num = 10;
				break; // 3
			case 0x70:
				temp_num = 11;
				break; //-
			}
		}
		while (P2 != 0xfb)
			;
		if (temp_num == 8 || temp_num == 9 || temp_num == 10) //如果按下的是'1','2'或'3'
		{
			if (j != 0)
			{
				write_com(0x01);
				clear_numoper();
				conti_light=0;
				j = 0;
			}
			num[num_number]= num[num_number]*10+table[temp_num];

			//如果没有输入计算符号
			if(num_number==0){
				if(shift_mode!=0){ 
					//如果是shift模式
					shift_compute_flag=1;
					hex_result=hex_result*16+hex_map(table[temp_num]);
				}
				else
					hex_result=hex_result*16+table[temp_num];
			}
		}
		else if (temp_num == 11) //如果按下的是'-'
		{
			if(num_number!=6)
				num_number++;
			oper[oper_number++]= '-';
		}
		// i = table1[num];	  //数据显示做准备
		// write_date(0x30 + i); //显示数据或操作符号
		// voice(50);
		after_key();
	}//if (P2 != 0xfb)

	P2 = 0xf7; // 1111 0111
	if (P2 != 0xf7)
	{
		delay(20);
		if (P2 != 0xf7)
		{
			temp = P2 & 0xf0;
			switch (temp)
			{
			case 0xe0:
				temp_num = 12;
				break; //清0键
			case 0xd0:
				temp_num = 13;
				break; //数字0
			case 0xb0:
				temp_num = 14;
				break; //等于键
			case 0x70:
				temp_num = 15;
				break; //加
			}
		}
		while (P2 != 0xf7)
			;
		switch (temp_num)
		{
		case 12://按下的是"清零"
		{
			j=1;
			clean_LCD();
			clear_numoper();
			conti_light_time=0;
			conti_light = 1;
			//***请继续***
			write_date('*');
			write_date('*');
			write_date('*');
			write_date('c');
			write_date('o');
			write_date('n');
			write_date('t');
			write_date('i');
			write_date('n');
			write_date('u');
			write_date('e');
			write_date('*');
			write_date('*');
			write_date('*');
			mode=1;
		}
		break;

		case 13:
		{				   //按下的是"0"
			if (j != 0)		//不加这个就可以去掉前导0
			{
				write_com(0x01);
				clear_numoper();
				conti_light=0;
				j = 0;
			}
			num[num_number]= num[num_number]*10;
			//如果没有输入计算符号存入十六进制里面
			if(num_number==0){
				if(shift_mode!=0){ 
					//如果是shift模式
					shift_compute_flag=1;
					hex_result=hex_result*16+hex_map(table[temp_num]);
				}
				else
					hex_result=hex_result*16+table[temp_num];
			}
			if(num_number==0&&shift_mode!=0) 
				write_date(0x41);
			else
				write_date(0x30);
		}
		break;

		case 14:
		{
			j = 1;			 //按下等于键，根据运算符号进行不同的算术处理
			//如果存在一个数字是按照shift输出(shift_compute_flag==1)
			//如果是在shiftmode下的输出(shift_mode!=0)
			if(shift_mode!=0 || shift_compute_flag==1){
				write_com(0x0c); //光标消失
				write_com(0x80 + 0x4f); //按下等于键，光标前进至第二行最后一个显示处
				write_com(0x04);		//设置从后住前写数据，每写完一个数据，光标后退一格
				temp_result=hex_result;
				while ( temp_result!= 0)
				{
					write_date(0x30 + temp_result % 10);
					temp_result = temp_result / 10;
				}
				if(hex_result<0)
					write_date(0x2d);
				if (hex_result==0)
					write_date(0x30 + 0);

				write_date(0x3d); //再写"="
			}
			else{
				compute_and_display();
			}
		}
		break;
		case 15:
		{
			write_date(0x30 + table1[temp_num]);
			if(num_number!=6)
				num_number++;
			oper[oper_number++]= '+';
		}
		break; //加键	 设置加标志fuhao=1;
		}

		//专门处理
		voice(50);
		standby=0;
		if(temp_num!=14){
			//如果不是等号，那就关注一下清除LED
			if(legal_result==0){
				light=1;
			}
		}
		if(temp_num==12){
			//一旦清零
			idle_time=0;
			standby=1;
			voice(50);
		}

	} // P2!=0xf7

}