

class Car
   {
     public:
          int speed;
   };

int main()
   {
  // Example of pointer to member data.
     int Car::*pSpeed_null = 0L;

  // int Car::*pSpeed = &Car::speed;


     return 0;
   }
