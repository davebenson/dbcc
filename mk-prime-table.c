#include <math.h>
#include <stdio.h>

int is_prime(unsigned x)
{
  if ((x & 1) == 0)
    return 0;
  unsigned sx = (unsigned)(int)sqrt(x);
  for (unsigned i = 3; i <= sx; i+= 2)
    if (x % i == 0)
      return 0;
  return 1;
}

int main()
{
  unsigned x = 5;
  for (;;)
    {
      while (!is_prime(x))
        x+= 2;
      printf("%u\n", x);
      unsigned long long xx = x;
      xx *= 2;
      xx += 1;
      x = xx;
      if (x < xx)                       // overflow!
        break;
    }
  return 0;

}
