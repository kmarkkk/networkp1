#include <stdio.h>
#include <string.h>

int main()
{
   char example[100];

   strcpy (example,"TechOnTheNet.com ");
   strcat (example,"is ");
   strcat (example,"over ");
   strcat (example,"10 ");
   strcat (example,"years ");
   strcat (example,"old.");
 printf(example);  
 printf("%u",sizeof(char));
   return 0;
}
