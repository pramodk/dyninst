void foo_outer(void);

__attribute__((noinline)) void main_caller(void) {
  foo_outer();
}

int main(void) {
  main_caller();
  return 0;
}
