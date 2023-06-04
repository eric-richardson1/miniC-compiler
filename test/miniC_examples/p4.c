extern void print(int);
extern int read();

int func(int i){
	int a;
	int b;
	
	b = i * 10;
	while (a < i){
		a = 10 + b;
		print(b);
	}
	return a;
}
