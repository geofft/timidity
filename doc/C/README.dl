====================== DYNAMIC INTERFACE ======================

 Interface modules that are specified with `-i' option can be
loadable modules by using dynamic link module.
 Place the shared library 

 if_<NAME>.so

on the SHARED_LIB_PATH directory. In this case <NAME> is the interface's
short name to be identified.
 TiMidity++ requires to this shared library the following function:
 (In this case <ID> is the interface's ID that are specified at `-i' option.)

ControlMode* interface_<ID>_loader(void)
{
	ControlMode* ctl;
	/* ... */

	return ctl;
}

 If the dynamic interface that are specified at -i<ID>, TiMidity++
loads if_<NAME>.so at the SHARED_LIB_PATH (this is the macro specified
in the Makefile) and calls a function in the shared library
interface_<ID>_loader().

 If a file

 if_<NAME>.txt

describing the information about the interface by 1 line is in the
SHARED_LIB_PATH, TiMidity++ displays this information when `-h'
option is specified.


WARNING:

 You shouldn't specify the interface you want to build as dynamic link
as enabled in Makefile. TiMidity++ searches its interfaces first from
interfaces statically linked with TiMidity++, and next from dynamic linked
interfaces.

