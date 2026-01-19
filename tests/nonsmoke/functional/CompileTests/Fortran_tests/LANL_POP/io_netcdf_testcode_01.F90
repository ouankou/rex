!|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

 module io_netcdf

!BOP
! !MODULE: io_netcdf
! !DESCRIPTION:
!  This module provides a generic input/output interface
!  for writing arrays in netCDF format.
!
! !REVISION HISTORY:
!  CVS:$Id: io_netcdf.F90,v 1.18 2002/12/20 10:21:09 pwjones Exp $
!  CVS:$Name: POP_2_0_1 $

! !USES:

   use kinds_mod
   use domain_size
   use domain
   use constants
   use communicate
   use boundary
   use broadcast
   use gather_scatter
   use exit_mod

   use io_types
   use netcdf

   implicit none
   private
   save

! !PUBLIC MEMBER FUNCTIONS:

   public :: open_read_netcdf,    &
             open_netcdf,         &
             close_netcdf,        &
             define_field_netcdf, &
             read_field_netcdf,   &
             write_field_netcdf

!EOP
!BOC
!-----------------------------------------------------------------------
!
!  module variables
!
!-----------------------------------------------------------------------


!EOC
!***********************************************************************

 contains

 subroutine open_read_netcdf()
 end subroutine open_read_netcdf

 subroutine open_netcdf()
 end subroutine open_netcdf

 subroutine close_netcdf()
 end subroutine close_netcdf


!***********************************************************************
!BOP
! !IROUTINE: define_field_netcdf
! !INTERFACE:

! subroutine define_field_netcdf(data_file, io_field)
 subroutine define_field_netcdf(io_field)

! !DESCRIPTION:
!  This routine defines an io field for a netCDF file.
!  When reading a file, the define routine will attempt to fill an
!  io field structure with meta-data information from the netCDF file.
!  When writing a file, it calls the appropriate netCDF routines
!  to define all the field attributes and assign a field id.
!
! !REVISION HISTORY:
!  same as module

! !INPUT/OUTPUT PARAMETERS:

!   type (datafile), intent (inout)  :: data_file       ! data file in which field contained

   type (io_field_desc), intent (inout) :: &
      io_field        ! field descriptor for this field

!EOP
!BOC
!-----------------------------------------------------------------------
!
!  local variables
!
!-----------------------------------------------------------------------

   character (80) :: &
      work_line,     &! workspace for manipulating input string
      comp_line,     &! comparison string
      att_name        ! attribute name

   integer (i4) :: &
      iostat,      &! status flag for netCDF calls
      ncid,        &! file id for netcdf file
      varid,       &! variable id for field
      ndims,       &! number of dimensions
      dimid,       &! dimension id
      n,           &! loop index
      ncount,      &! num additional attributes
      nsize,       &! length of character strings
      itype,       &! netCDF data type
      num_atts,    &! number of variable attributes
      att_ival,    &! temp for integer attribute
      ncvals,      &! counter for number of character attributes
      nlvals,      &! counter for number of logical   attributes
      nivals,      &! counter for number of integer   attributes
      nrvals,      &! counter for number of real      attributes
      ndvals        ! counter for number of double    attributes

   logical (log_kind) ::    &
      att_lval      ! temp for logical attribute

   real (r4) ::    &
      att_rval      ! temp for real attribute

   real (r4) ::    &
      att_dval      ! temp for double attribute

   logical (log_kind) :: &
      define_error       ! error flag

   integer :: nf90_noerr


! DQ ((9/12/2010): Comment this out as a test.
            iostat = NF90_INQ_DIMID(ncid,att_name,dimid)
            iostat = NF90_INQ_DIMID(ncid,io_field%field_dim(n)%name,dimid)

!-----------------------------------------------------------------------
!
!  make sure file has been opened
!
!-----------------------------------------------------------------------

!-----------------------------------------------------------------------
!
!  for input files, get the variable id and determine number of field
!  attributes
!
!-----------------------------------------------------------------------

!   if (data_file%readonly) then
!   else ! output file
!      if (my_task == master_task) then
!         do n = 1,ndims
            if (iostat /= NF90_NOERR) then ! dimension not yet defined
            else
               io_field%field_dim(n)%id = dimid
            end if
!         end do

!         if (iostat /= NF90_NOERR) then ! variable was not yet defined
!         end if
!      end if ! master task

!   endif ! input/output file

!-----------------------------------------------------------------------
!EOC

 end subroutine define_field_netcdf

 subroutine write_field_netcdf()

 end subroutine write_field_netcdf

 subroutine read_field_netcdf()

 end subroutine read_field_netcdf

 subroutine check(status)

   integer (i4), intent (in) ::  status  ! status returned by netCDF call

 end subroutine check

!***********************************************************************

 end module io_netcdf

!|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
