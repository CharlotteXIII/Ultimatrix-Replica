import os

Folder = r"C:\Users\User\Desktop\MyWork\MyProjects\Ultimatrix Replica\imgAliens"
Prefix = "alien"

#Getpart, index numbers and file extension then return them
#Get the info into vars
def get_part_and_index(filename):
 
    name_part, extension = os.path.splitext(filename)
    extension = extension.replace(".", "")
 
    after_part = name_part.split("part_")[-1]
    print("after_part = \n",after_part)

    numbers = after_part.split("_")
    print("numbers = \n",numbers)
 
    part_number = int(numbers[0])
    index_number = int(numbers[1])

    return part_number, index_number, extension
 
 
#Rename The files
def main():
 
    files = []
 
    for entry in os.scandir(Folder):
        #check subfolder
        if not entry.is_file():
            continue
 
        part_number, index_number, extension = get_part_and_index(entry.name)
 
        files.append({
            "old_name": entry.name,
            "part": part_number,
            "index": index_number,
            "ext": extension,
        })
 
    # sort files relate to part number (Prevent 10 before 1)
    files.sort(key=lambda info: (info["part"], info["index"]))
 
    print(f"Total files {len(files)} \n")
 
    # loop change each files
    for info in files:
        new_name = f"{Prefix}_{info['part']}_{info['index']}.{info['ext']}"
 
        old_full_path = os.path.join(Folder, info["old_name"])
        new_full_path = os.path.join(Folder, new_name)
 
        os.rename(old_full_path, new_full_path)
        print(f"Name changed: {info['old_name']}  to  {new_name}") 
 
if __name__ == "__main__":
    main()