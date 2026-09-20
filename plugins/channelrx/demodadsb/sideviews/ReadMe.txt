SECTION FROM THE MANUAL RELATING TO THE USE OF OPERATOR FLAGS

The operator column can be used to display a graphic for each aircraft, typically to show its
operator. Operator flag graphic files must be in .bmp format and must be placed in an
OperatorFlags subdirectory. To set an aircraft to display the operator flag, enter the name of
the file minus the .bmp extension into the OperatorFlagCode of the Edit Aircraft Details dialog
box.

For example, if you enter “BAW” into the OperatorFlagCode field of an aircraft, and you have
previously placed the file “BAW.bmp” into the directory
C:\Program Files\Kinetic\BaseStation\OperatorFlags, then the baw bitmap will be displayed in the 
Operator column for that aircraft.

Note: We do not envisage users typically entering these codes using this interface. Instead, we
have provided this feature so that those existing third party applications that use BaseStation’s
database interface to write registration data into the database can write in flag codes matching
existing flag data sets, allowing the users of those third party products to have operator flags 
automatically appear.