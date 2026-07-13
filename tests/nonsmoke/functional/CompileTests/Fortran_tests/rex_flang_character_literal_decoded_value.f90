program character_literal_decoded_value
  implicit none
  character(len=*), parameter :: apostrophe = 'isn''t'
  character(len=*), parameter :: quotation = "say ""yes"""

  if (apostrophe /= "isn't") error stop
  if (quotation /= 'say "yes"') error stop
end program character_literal_decoded_value
