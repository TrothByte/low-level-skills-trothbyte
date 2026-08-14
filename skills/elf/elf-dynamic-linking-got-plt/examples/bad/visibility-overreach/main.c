extern int counter;

extern void bump(void);
extern int get(void);

int main(void)
{
    bump();
    bump();
    return counter == 2 ? 0 : 1;
}
