      program assign_goto_entry
      integer i, k
      assign 100 to i
      go to i
100   continue
      assign 200 to i
      go to i, (200,300)
200   continue
300   continue
      k = 2
      go to (400,500) k
400   continue
500   continue
      call suba(1)
      end

      subroutine suba(n)
      integer n
      entry subb(n)
      return
      end
