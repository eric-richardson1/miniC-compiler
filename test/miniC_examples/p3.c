extern void print(int);
extern int read();

int func(int i){
	int a;
	int b;
	b = 90;
	while (b < i){
		
		a = 10 + b;
		b = b * i;
	}

	while (b < i){

		b = b * 10;
	}
}
