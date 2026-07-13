function rex_dynamic_character_result_predeclaration(character_set) result(value)
  implicit none
  character, intent(in) :: character_set
  character(len=scan('123456789', character_set)) :: value

  value = 'x'
end function rex_dynamic_character_result_predeclaration
