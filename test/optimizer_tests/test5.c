extern void print(int);
extern int read();

int test() {
	int n;
	int c;
	n = 10;
	while (n < 10) {
		n = n + 1;
		if (n == 9) {
			c = 4;
		}
	}
	return(n + c);
}