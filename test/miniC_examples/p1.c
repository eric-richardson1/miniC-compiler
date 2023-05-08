extern void print(int);
extern int read();

int func(int i){
	int a;
	int b;
	a = 10;
	b = 5;
	a = -10 + b;
	
	
  if (a < i){
		while (b < i){
			b = b + 20;
		}
		a = - 10 + b;
	}
	else {
		if (b < i) 
			b = a;
	}
	return(a + b);
}
