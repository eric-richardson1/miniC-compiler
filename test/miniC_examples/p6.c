
extern void print(int);
extern int read();

int func(int a) {
    int loc1;
    int loc2;
    int i;
    int j;
    
    loc1 = 10;
    loc2 = a * 10;
    
    if (loc1 > loc2) 
        print(loc1);

    
    else
        print(loc2);

    i = 0;
    while (i < loc1){
        i = i + 1;
        j = loc2 + i;
        loc2 = j;
    }
    
    return(loc1);
    
}
