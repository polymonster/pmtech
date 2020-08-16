namespace pen
{
	extern void* user_entry(void* params);
}

int main() {
  pen::user_entry(nullptr);
  return 0;
}

int hello()
{
	return 0;
}