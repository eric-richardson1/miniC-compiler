extern void print(int);
extern int read();

int func() {
	int n;
	int c;
	n = 0;
	c = 40;
	while (n < 10) {
		n = n + 1;
		if (n == 9) {
			c = n * c;
			print(c);
		}
	}
	return(n);
}