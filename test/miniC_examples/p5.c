extern void print(int);
extern int read();

int func(int i){
	int a;
	int b;
	a = 0;
	b = 0;
	
	while (a < i){

		a = read();
		b = 10 + a;
	}
	return(a);
}
