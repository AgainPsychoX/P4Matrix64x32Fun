import io
from contextlib import redirect_stdout
from subprocess import Popen, PIPE

# PlatformIO injected stuff (I wish it was typed... https://github.com/platformio/platformio-docs/issues/342)
Import('env' ,'projenv') # type: ignore
env, projenv, DefaultEnvironment = env, projenv, DefaultEnvironment # type: ignore

def after_build(source, target, env):
	# Start with objdump binary name
	cmd = [env['OBJCOPY'].replace('objcopy', 'objdump')]

	# Add architecture if any detected
	arch = None
	for x in env['LINKFLAGS']:
		if x.startswith('-march='):
			arch = x.split('=')[1]
			break
	if arch:
		cmd += ['-m', arch]

	# Add common flags & input file path
	cmd += [
		'-d', '-S', '-C', '--file-start-context', '-w',
		env.subst('./.pio/build/${PIOENV}/${PROGNAME}.elf')
	]

	# Start the process, redirecting to a file
	with open(env.subst('${PROGNAME}.disasm'), 'w') as file:
		p = Popen(cmd, stdout=file, universal_newlines=True)

env.AddCustomTarget(
	'disasm',
	'${BUILD_DIR}/${PROGNAME}.elf',
	after_build,
	title='Disasm',
	description='Generate a disassembly file on demand',
	always_build=True
)
