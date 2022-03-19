#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
	cout << "How many fibonacci numbers do you want?";
	int nums;
	cin >> nums;
	int a = 0;
	int b = 1;
	while(nums > 0)
	{
		cout << a;
		int c = a + b;
		a = b;
		b = c;
		nums = nums - 1;
	}
	return 0;
}
