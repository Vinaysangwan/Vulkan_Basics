#include "application.h"

int main()
{
  Application app;
  if (!application_init(&app))
  {
    return -1;
  }

  application_run(&app);

  application_shutdown(&app);
  
  return 0;
}
