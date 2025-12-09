import os

# Directory containing the files
directory = './'

# Keywords to keep in filenames
keywords = ['ao', 'albedo', 'roughness', 'normal', 'metallic', 'height']

# Function to rename files
def rename_files(directory, keywords):
    for filename in os.listdir(directory):
        # Extract the file extension
        ext = os.path.splitext(filename)[1]
        
        # Find and keep the keywords in the filename
        new_name_parts = [kw for kw in keywords if kw in filename.lower()]
        
        # Join the keywords with underscores
        new_name = '_'.join(new_name_parts)
        
        # Create the full new name
        new_filename = f"{new_name}{ext}"
        
        # Full path for the old and new filenames
        old_file = os.path.join(directory, filename)
        new_file = os.path.join(directory, new_filename)
        
        # Rename the file if new_name is not empty
        if new_name:
            os.rename(old_file, new_file)
            print(f"Renamed '{filename}' to '{new_filename}'")
        else:
            print(f"Skipping '{filename}' as it contains no keywords")

# Run the function
rename_files(directory, keywords)
