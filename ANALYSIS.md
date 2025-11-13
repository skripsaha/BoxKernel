# TagFS GPF Analysis & Fix Plan

## Current Status
- TagFS allocation: vmalloc (512KB in kernel heap)
- Superblock: tagfs_storage[0]
- Inode table: tagfs_storage[inode_table_block]
- Disk sync: DISABLED (causes corruption)
- **Problem**: GPF when running `ls` command

## Root Cause Analysis

### 1. Pointer Initialization Chain
```c
// Step 1: Allocate storage (tagfs_init:552)
tagfs_storage = (uint8_t (*)[TAGFS_BLOCK_SIZE])vmalloc(storage_size);

// Step 2: Set superblock pointer (tagfs_init:566)
global_tagfs.superblock = (TagFSSuperblock*)tagfs_storage[0];

// Step 3: Format filesystem (tagfs_init:606)
tagfs_format(TAGFS_MEM_BLOCKS);
  // → This memsets entire storage INCLUDING superblock!
  // → Then writes to global_tagfs.superblock which points to tagfs_storage[0]

// Step 4: Set inode_table pointer (tagfs_init:617)
global_tagfs.inode_table = (FileInode*)tagfs_storage[global_tagfs.superblock->inode_table_block];
```

### 2. Potential Issues
a) **Pointer Arithmetic**: `tagfs_storage[i]` with pointer-to-array type
b) **Uninitialized access**: inode_table might point to wrong address
c) **Memory corruption**: Disk sync writes corrupt data back

### 3. GPF Location
Most likely in `cmd_ls:305`:
```c
FileInode* inode = &global_tagfs.inode_table[i];
```

If `global_tagfs.inode_table` is NULL or invalid, this causes GPF.

## Fix Strategy

### Phase 1: Add Diagnostic Output
- Print all pointer values during init
- Validate each step
- Catch exact GPF location

### Phase 2: Fix Pointer Issues
- Ensure proper alignment
- Validate pointer before use
- Add bounds checking

### Phase 3: Fix Disk Sync
- Debug corruption issue
- Enable safe disk persistence
- Add sync validation

### Phase 4: Add Safety
- NULL pointer checks
- Bounds validation
- Error recovery
