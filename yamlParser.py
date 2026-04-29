import re
import subprocess

def clean_function_name(mangled):
    # 1. Handle Parallel Thread Pool Lambdas (which preserve the runSystemImpl name)
    if "runSystemImpl" in mangled:
        exec_policy = "Exec::Par" if "IL4Exec1E" in mangled else "Exec::Seq"
        
        components = []
        comp_match = re.search(r'IL4Exec\dEJ([0-9A-Za-z_]+?)E[R]?Z', mangled)
        if comp_match:
            comp_str = comp_match.group(1)
            while comp_str:
                num_match = re.match(r'^(\d+)', comp_str)
                if not num_match: break
                length = int(num_match.group(1))
                num_len = len(num_match.group(1))
                components.append(comp_str[num_len:num_len+length])
                comp_str = comp_str[num_len+length:]
                
        caller = "Unknown System"
        caller_matches = re.finditer(r'[R]?Z(\d+)([A-Za-z_][A-Za-z0-9_]*)', mangled)
        for m in caller_matches:
            length = int(m.group(1))
            name = m.group(2)[:length]
            if "runSystemImpl" not in name and "Manager" not in name:
                caller = name
                break
                
        comp_text = ", ".join(components) if components else "Empty"
        return f"runSystemImpl<{exec_policy}, {comp_text}> (Inside: {caller})"
        
    # 2. Handle Inlined Sequential Systems (Compiler strips runSystemImpl, leaves Caller Name)
    match = re.search(r'_Z(\d+)([A-Za-z_][A-Za-z0-9_]*)', mangled)
    if match:
        length = int(match.group(1))
        name = match.group(2)[:length]
        return f"Inlined ECS System Loop (Inside: {name})"
        
    # 3. Fallback to c++filt for standard C++ library stuff
    try:
        res = subprocess.run(['c++filt', mangled], capture_output=True, text=True)
        if res.returncode == 0:
            return res.stdout.strip()
    except:
        pass
        
    return mangled


def parse_all_vectorized(filepath):
    try:
        with open(filepath, 'r') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: Could not find '{filepath}'")
        return

    # Split by standard YAML document separators
    docs = content.split('---')
    print(f"Found {len(docs)} total records. Searching for ALL vectorized loops...\n")
    
    passed_count = 0
    for doc in docs:
        # Check for successful pass AND the loop-vectorize identifier
        if '!Passed' in doc and 'loop-vectorize' in doc:
            passed_count += 1
            
            # FIX: Stop capturing at the first whitespace/newline to prevent grabbing the whole file
            func_match = re.search(r"Function:\s+['\"]?([^\s'\"]+)['\"]?", doc)
            mangled_func = func_match.group(1) if func_match else "Unknown Function"
            
            clean_name = clean_function_name(mangled_func)
            
            # Extract Args block securely
            args_match = re.search(r"Args:\s*\n(.*)", doc, re.DOTALL)
            clean_args = ""
            if args_match:
                for line in args_match.group(1).split('\n'):
                    # FIX: Allow for trailing whitespace and capture safely
                    val_match = re.search(r"-\s+[A-Za-z0-9_]+:\s+['\"]?(.*?)['\"]?\s*$", line)
                    if val_match:
                        clean_args += val_match.group(1)
                        
            print("-" * 80)
            print(f"✅ PASSED: {clean_name}")
            print(f"DETAILS : {clean_args.strip()}")
            
    print("-" * 80)
    print(f"Total Successfully Vectorized Loops: {passed_count}")

if __name__ == "__main__":
    parse_all_vectorized('build/CMakeFiles/texnologia-polymeswn.dir/src/main.cpp.opt.yaml')