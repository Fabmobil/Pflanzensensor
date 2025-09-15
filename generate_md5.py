Import('env')
import hashlib
import os

def generate_md5(source, target, env):
    try:
        input_file = str(target[0])
        if not os.path.exists(input_file):
            print(f"Warning: File not found: {input_file}")
            return

        with open(input_file, 'rb') as f:
            file_md5 = hashlib.md5(f.read()).hexdigest()

        md5_path = input_file + '.md5'
        with open(md5_path, 'w') as f:
            f.write(file_md5)

        print(f"MD5 hash generated for {os.path.basename(input_file)}: {file_md5}")

    except Exception as e:
        print(f"Error generating MD5: {str(e)}")

# Add post-action for both firmware and filesystem
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", generate_md5)
env.AddPostAction("buildfs", lambda *a, **kw: generate_md5(None, [env.File("$BUILD_DIR/littlefs.bin")], env))
