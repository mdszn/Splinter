struct descriptors
{
  int read_in;
  int write_out;
};

void * thrd_reader(void *);
void * thrd_wait(void *);
