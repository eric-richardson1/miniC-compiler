extern void print(int);
extern int read();


int func(int i){
	int a;
	int b;
	a = read();
	b = a + i;
	print(a);	
	return (a);	
}