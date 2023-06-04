extern void print(int);
extern int read();

int func(int i){
	int a;
	int b;
	
	while (a < i){

		a = read();
		b = 10 + a;
	}
	return(a);
}
