#include <stdio.h>
#include <math.h>

double newt_coeffs[58][58];

int main()
{
    int i, j, n = 57;
    int sign;

    newt_coeffs[0][0] = 1;
    for (i = 0; i <= n; i++)
    {
    	newt_coeffs[i][0] = 1;
    	newt_coeffs[i][i] = 1;

	if (i > 1)
	{
	    newt_coeffs[i][0] = newt_coeffs[i-1][0] / i;
	    newt_coeffs[i][i] = newt_coeffs[i-1][0] / i;
	}

    	for (j = 1; j < i; j++)
    	{
    	    newt_coeffs[i][j] = newt_coeffs[i-1][j-1] + newt_coeffs[i-1][j];

	    if (i > 1)
	    	newt_coeffs[i][j] /= i;
	}
    }
    for (i = 0; i <= n; i++)
    	for (j = 0, sign = pow(-1, i); j <= i; j++, sign *= -1)
    	    newt_coeffs[i][j] *= sign;


    for (i = 0; i <= n; i++)
	for (j = 0; j <= n; j++)
	    printf("%2.32g,\n", newt_coeffs[i][j]);
    return 0;
}
