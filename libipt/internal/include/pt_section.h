/*
 * Copyright (c) 2013-2015, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PT_SECTION_H
#define PT_SECTION_H

#include <stdint.h>

#if defined(FEATURE_THREADS)
#  include <threads.h>
#endif /* defined(FEATURE_THREADS) */


/* A section of contiguous memory loaded from a file. */
struct pt_section {
	/* The name of the file. */
	char *filename;

	/* The offset into the file. */
	uint64_t offset;

	/* The (adjusted) size in bytes.  The size is truncated to match the
	 * actual file size.
	 */
	uint64_t size;

	/* A pointer to OS-specific file status for detecting changes.
	 *
	 * The status is initialized on first pt_section_map() and will be
	 * left in the section until the section is destroyed.  This field
	 * is owned by the OS-specific mmap-based section implementation.
	 */
	void *status;

	/* A pointer to implementation-specific mapping information - NULL if
	 * the section is currently not mapped.
	 *
	 * This field is set in pt_section_map() and owned by the mapping
	 * implementation.
	 */
	void *mapping;

	/* A pointer to the unmap function - NULL if the section is currently
	 * not mapped.
	 *
	 * This field is set in pt_section_map() and owned by the mapping
	 * implementation.
	 */
	int (*unmap)(struct pt_section *sec);

	/* A pointer to the read function - NULL if the section is currently
	 * not mapped.
	 *
	 * This field is set in pt_section_map() and owned by the mapping
	 * implementation.
	 */
	int (*read)(const struct pt_section *sec, uint8_t *buffer,
		    uint16_t size, uint64_t offset);

#if defined(FEATURE_THREADS)
	/* A lock protecting this section.
	 *
	 * Most operations do not require the section to be locked.  All
	 * actual locking should be handled by pt_section_* functions.
	 */
	mtx_t lock;
#endif /* defined(FEATURE_THREADS) */

	/* The number of current users.  The last user destroys the section. */
	uint16_t ucount;

	/* The number of current mappers.  The last unmaps the section. */
	uint16_t mcount;
};

/* Create a section.
 *
 * The returned section describes the contents of @file starting at @offset
 * for @size bytes.
 *
 * If @file is shorter than the requested @size, the section is silently
 * truncated to the size of @file.
 *
 * If @offset lies beyond the end of @file, no section is created.
 *
 * The returned section is not mapped and starts with a user count of one.
 *
 * Returns a new section on success, NULL otherwise.
 */
extern struct pt_section *pt_mk_section(const char *file, uint64_t offset,
					uint64_t size);

/* Lock a section.
 *
 * Locks @section.  The section must not be locked.
 *
 * Returns a new section on success, NULL otherwise.
 * Returns -pte_bad_lock on any locking error.
 */
int pt_section_lock(struct pt_section *section);

/* Unlock a section.
 *
 * Unlocks @section.  The section must be locked.
 *
 * Returns a new section on success, NULL otherwise.
 * Returns -pte_bad_lock on any locking error.
 */
int pt_section_unlock(struct pt_section *section);

/* Add another user.
 *
 * Increments the user count of @section.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @section is NULL.
 * Returns -pte_internal if the user count would overflow.
 * Returns -pte_bad_lock on any locking error.
 */
extern int pt_section_get(struct pt_section *section);

/* Remove a user.
 *
 * Decrements the user count of @section.  Destroys the section if the
 * count reaches zero.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @section is NULL.
 * Returns -pte_internal if the user count is already zero.
 * Returns -pte_bad_lock on any locking error.
 */
extern int pt_section_put(struct pt_section *section);

/* Return the filename of @section. */
extern const char *pt_section_filename(const struct pt_section *section);

/* Return the size of the section in bytes. */
extern uint64_t pt_section_size(const struct pt_section *section);

/* Create the OS-specific file status.
 *
 * On success, allocates a status object, provides a pointer to it in @pstatus
 * and provides the file size in @psize.
 *
 * The status object will be free()'ed when its section is.
 *
 * This function is implemented in the OS-specific section implementation.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @pstatus, @psize, or @filename is NULL.
 * Returns -pte_bad_image if @filename can't be opened.
 * Returns -pte_nomem if the status object can't be allocated.
 */
extern int pt_section_mk_status(void **pstatus, uint64_t *psize,
				const char *filename);

/* Map a section.
 *
 * Maps @section into memory.  Mappings are use-counted.  The number of
 * pt_section_map() calls must match the number of pt_section_unmap()
 * calls.
 *
 * This function is implemented in the OS-specific section implementation.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @section is NULL.
 * Returns -pte_bad_image if @section changed or can't be opened.
 * Returns -pte_bad_lock on any locking error.
 * Returns -pte_nomem if @section can't be mapped into memory.
 */
extern int pt_section_map(struct pt_section *section);

/* Unmap a section.
 *
 * Unmaps @section from memory.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @section is NULL.
 * Returns -pte_bad_lock on any locking error.
 * Returns -pte_internal if @section has not been mapped.
 */
extern int pt_section_unmap(struct pt_section *section);

/* Read memory from a section.
 *
 * Reads at most @size bytes from @section at @offset into @buffer.  @section
 * must be mapped.
 *
 * Returns the number of bytes read on success, a negative error code otherwise.
 * Returns -pte_invalid if @section or @buffer are NULL.
 * Returns -pte_nomap if @offset is beyond the end of the section.
 */
extern int pt_section_read(const struct pt_section *section, uint8_t *buffer,
			   uint16_t size, uint64_t offset);

#endif /* PT_SECTION_H */
