extern int read();
extern void print(int);

int func(int n){
	int sum;
	int i;
	int a;
	sum = 0;
	i = 0;
	
	while (i < n){ 
		a = read();
		sum = sum + a;
		i = i + 1;
	}
	
	return sum;
}
