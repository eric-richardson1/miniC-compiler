extern void print(int);
extern int read();

int func(){
 	int a;
	int b;
	int c;
	
	a = 10;
	b = 20;	
	c = 30 + 40;
	if (a < 20) {
		a = c * 10;
		b = c * 10;
		
		return(a);
		a = c + 1;
		c = 0;
		c = 99 * a;
	}

	return(c); 
}
