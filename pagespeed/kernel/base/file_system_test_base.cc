/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include "pagespeed/kernel/base/file_system_test_base.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

// TODO(huibao): Rename FileSystemTest to FileSystemTestBase.

FileSystemTest::FileSystemTest()
    : test_tmpdir_(StrCat(GTestTempDir(), "/file_system_test_base")) {}
FileSystemTest::~FileSystemTest() {}

// Used by TestDirInfo to make sure vector of FileInfos can be compared
// consistently
struct CompareByName {
 public:
  bool operator()(const FileSystem::FileInfo& one,
                  const FileSystem::FileInfo& two) const {
    return one.name < two.name;
  }
};

GoogleString FileSystemTest::WriteNewFile(const StringPiece& suffix,
                                          const GoogleString& content) {
  GoogleString filename = StrCat(test_tmpdir(), suffix);

  // Make sure we don't read an old file.
  DeleteRecursively(filename);
  EXPECT_TRUE(file_system()->WriteFile(filename.c_str(), content, &handler_));

  return filename;
}

// Check that a file has been read.
void FileSystemTest::CheckRead(const GoogleString& filename,
                               const GoogleString& expected_contents) {
  GoogleString buffer;
  ASSERT_TRUE(file_system()->ReadFile(filename.c_str(), &buffer, &handler_));
  EXPECT_EQ(buffer, expected_contents);
}

// Check that a file has been read.
void FileSystemTest::CheckInputFileRead(const GoogleString& filename,
                                        const GoogleString& expected_contents) {
  FileSystem::InputFile* file =
      file_system()->OpenInputFile(filename.c_str(), &handler_);
  ASSERT_TRUE(file != nullptr);
  GoogleString buffer;
  ASSERT_TRUE(file_system()->ReadFile(file, &buffer, &handler_));
  EXPECT_EQ(buffer, expected_contents);
}

// Make sure we can no longer read the file by the old name.  Note
// that this will spew some error messages into the log file, and
// we can add a null_message_handler implementation to
// swallow them, if they become annoying.
void FileSystemTest::CheckDoesNotExist(const GoogleString& filename) {
  GoogleString read_buffer;
  EXPECT_FALSE(file_system()->ReadFile(filename.c_str(), &read_buffer,
                                       &handler_));
  EXPECT_TRUE(file_system()->Exists(filename.c_str(), &handler_).is_false());
}

// Write a named file, then read it.
void FileSystemTest::TestWriteRead() {
  GoogleString filename = StrCat(test_tmpdir(), "/write.txt");
  GoogleString msg("Hello, world!");

  DeleteRecursively(filename);
  FileSystem::OutputFile* ofile = file_system()->OpenOutputFile(
      filename.c_str(), &handler_);
  ASSERT_TRUE(ofile != nullptr);
  EXPECT_TRUE(ofile->Write(msg, &handler_));
  EXPECT_TRUE(file_system()->Close(ofile, &handler_));
  CheckRead(filename, msg);
  CheckInputFileRead(filename, msg);

  // Now check that if we set a low limit we can't read it.
  GoogleString buffer;
  EXPECT_FALSE(file_system()->ReadFile(filename.c_str(),
                                       5,  // limit to 5 bytes
                                       &buffer, &handler_));

  FileSystem::InputFile* ifile =
      file_system()->OpenInputFile(filename.c_str(), &handler_);
  ASSERT_TRUE(ifile != nullptr);
  EXPECT_FALSE(file_system()->ReadFile(ifile,
                                       5,  // limit to 5 bytes
                                       &buffer, &handler_));
}

// Write a temp file, then read it.
void FileSystemTest::TestTemp() {
  GoogleString prefix = StrCat(test_tmpdir(), "/temp_prefix");
  FileSystem::OutputFile* ofile = file_system()->OpenTempFile(
      prefix, &handler_);
  ASSERT_TRUE(ofile != nullptr);
  GoogleString filename(ofile->filename());
  GoogleString msg("Hello, world!");
  EXPECT_TRUE(ofile->Write(msg, &handler_));
  EXPECT_TRUE(file_system()->Close(ofile, &handler_));

  CheckRead(filename, msg);
}

// Write a temp file, close it, append to it, then read it.
void FileSystemTest::TestAppend() {
  GoogleString prefix = StrCat(test_tmpdir(), "/temp_prefix");
  FileSystem::OutputFile* ofile = file_system()->OpenTempFile(
      prefix, &handler_);
  ASSERT_TRUE(ofile != nullptr);
  const GoogleString filename(ofile->filename());
  EXPECT_TRUE(ofile->Write("Hello", &handler_));
  EXPECT_TRUE(file_system()->Close(ofile, &handler_));
  ofile = file_system()->OpenOutputFileForAppend(filename.c_str(), &handler_);
  EXPECT_TRUE(ofile->Write(" world!", &handler_));
  EXPECT_TRUE(file_system()->Close(ofile, &handler_));

  CheckRead(filename, "Hello world!");
}

// Write a temp file, rename it, then read it.
void FileSystemTest::TestRename() {
  GoogleString from_text = "Now is time time";
  GoogleString to_file = StrCat(test_tmpdir(), "/to.txt");
  DeleteRecursively(to_file);

  GoogleString from_file = WriteNewFile("/from.txt", from_text);
  ASSERT_TRUE(file_system()->RenameFile(from_file.c_str(), to_file.c_str(),
                                        &handler_));

  CheckDoesNotExist(from_file);
  CheckRead(to_file, from_text);
}

// Write a file and successfully delete it.
void FileSystemTest::TestRemove() {
  GoogleString filename = WriteNewFile("/remove.txt", "Goodbye, world!");
  ASSERT_TRUE(file_system()->RemoveFile(filename.c_str(), &handler_));
  CheckDoesNotExist(filename);
}

// Write a file and check that it exists.
void FileSystemTest::TestExists() {
  GoogleString filename = WriteNewFile("/exists.txt", "I'm here.");
  ASSERT_TRUE(file_system()->Exists(filename.c_str(), &handler_).is_true());
}

// Create a file along with its directory which does not exist.
void FileSystemTest::TestCreateFileInDir() {
  GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir");
  DeleteRecursively(dir_name);
  GoogleString filename = dir_name + "/file-in-dir.txt";

  FileSystem::OutputFile* file =
      file_system()->OpenOutputFile(filename.c_str(), &handler_);
  ASSERT_TRUE(file != nullptr);
  file_system()->Close(file, &handler_);
}


// Make a directory and check that files may be placed in it.
void FileSystemTest::TestMakeDir() {
  GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir");
  DeleteRecursively(dir_name);
  GoogleString filename = dir_name + "/file-in-dir.txt";

  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  // ... but we can open a file after we've created the directory.
  FileSystem::OutputFile* file =
      file_system()->OpenOutputFile(filename.c_str(), &handler_);
  ASSERT_TRUE(file != nullptr);
  file_system()->Close(file, &handler_);
}

// Make a directory and then remove it.
void FileSystemTest::TestRemoveDir() {
  // mem_file_system depends on dir_names ending with a '/'
  GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir/");
  DeleteRecursively(dir_name);
  GoogleString filename = dir_name + "file-in-dir.txt";

  EXPECT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  EXPECT_TRUE(file_system()->Exists(dir_name.c_str(), &handler_).is_true());

  // First test that non-empty directories don't get deleted
  FileSystem::OutputFile* file =
      file_system()->OpenOutputFile(filename.c_str(), &handler_);
  EXPECT_TRUE(file != nullptr);
  file_system()->Close(file, &handler_);
  EXPECT_FALSE(file_system()->RemoveDir(dir_name.c_str(), &handler_));
  EXPECT_TRUE(file_system()->Exists(filename.c_str(), &handler_).is_true());
  EXPECT_TRUE(file_system()->Exists(dir_name.c_str(), &handler_).is_true());

  // Then test that empty directories do get deleted
  EXPECT_TRUE(file_system()->RemoveFile(filename.c_str(), &handler_));
  EXPECT_TRUE(file_system()->RemoveDir(dir_name.c_str(), &handler_));
  EXPECT_TRUE(file_system()->Exists(filename.c_str(), &handler_).is_false());
  EXPECT_TRUE(file_system()->Exists(dir_name.c_str(), &handler_).is_false());
}

// Make a directory and check that it is a directory.
void FileSystemTest::TestIsDir() {
  GoogleString dir_name = StrCat(test_tmpdir(), "/this_is_a_dir");
  DeleteRecursively(dir_name);

  // Make sure we don't think the directory is there when it isn't ...
  ASSERT_TRUE(file_system()->IsDir(dir_name.c_str(), &handler_).is_false());
  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  // ... and that we do think it's there when it is.
  ASSERT_TRUE(file_system()->IsDir(dir_name.c_str(), &handler_).is_true());

  // Make sure that we don't think a regular file is a directory.
  GoogleString filename = dir_name + "/this_is_a_file.txt";
  GoogleString content = "I'm not a directory.";
  ASSERT_TRUE(file_system()->WriteFile(filename.c_str(), content, &handler_));
  ASSERT_TRUE(file_system()->IsDir(filename.c_str(), &handler_).is_false());
}

// Recursively make directories and check that it worked.
void FileSystemTest::TestRecursivelyMakeDir() {
  GoogleString base = StrCat(test_tmpdir(), "/base");
  GoogleString long_path = base + "/dir/of/a/really/deep/hierarchy";
  DeleteRecursively(base);

  // Make sure we don't think the directory is there when it isn't ...
  ASSERT_TRUE(file_system()->IsDir(long_path.c_str(), &handler_).is_false());
  ASSERT_TRUE(file_system()->RecursivelyMakeDir(long_path, &handler_));
  // ... and that we do think it's there when it is.
  ASSERT_TRUE(file_system()->IsDir(long_path.c_str(), &handler_).is_true());
}

// Check that we cannot create a directory we do not have permissions for.
// Note: depends upon root dir not being writable.
void FileSystemTest::TestRecursivelyMakeDir_NoPermission() {
  GoogleString base = "/bogus-dir";
  GoogleString path = base + "/no/permission/to/make/this/dir";

  // Make sure the bogus bottom level directory is not there.
  ASSERT_TRUE(file_system()->Exists(base.c_str(), &handler_).is_false());
  // We do not have permission to create it.
  ASSERT_FALSE(file_system()->RecursivelyMakeDir(path, &handler_));
}

// Check that we cannot create a directory below a file.
void FileSystemTest::TestRecursivelyMakeDir_FileInPath() {
  GoogleString base = StrCat(test_tmpdir(), "/file-in-path");
  GoogleString filename = base + "/this-is-a-file";
  GoogleString bad_path = filename + "/some/more/path";
  DeleteRecursively(base);
  GoogleString content = "Your path must end here. You shall not pass!";

  ASSERT_TRUE(file_system()->MakeDir(base.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(filename.c_str(), content, &handler_));
  ASSERT_FALSE(file_system()->RecursivelyMakeDir(bad_path, &handler_));
}

void FileSystemTest::TestListContents() {
  GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir");
  DeleteRecursively(dir_name);
  GoogleString filename1 = dir_name + "/file-in-dir.txt";
  GoogleString filename2 = dir_name + "/another-file-in-dir.txt";
  GoogleString content = "Lorem ipsum dolor sit amet";

  StringVector mylist;

  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(filename1.c_str(), content, &handler_));
  ASSERT_TRUE(file_system()->WriteFile(filename2.c_str(), content, &handler_));
  EXPECT_TRUE(file_system()->ListContents(dir_name, &mylist, &handler_));
  EXPECT_EQ(size_t(2), mylist.size());
  // Make sure our filenames are in there
  EXPECT_FALSE(filename1.compare(mylist.at(0))
               && filename1.compare(mylist.at(1)));
  EXPECT_FALSE(filename2.compare(mylist.at(0))
               && filename2.compare(mylist.at(1)));
}

void FileSystemTest::TestAtime() {
  GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir");
  DeleteRecursively(dir_name);
  GoogleString filename1 = "file-in-dir.txt";
  GoogleString filename2 = "another-file-in-dir.txt";
  GoogleString full_path1 = dir_name + "/" + filename1;
  GoogleString full_path2 = dir_name + "/" + filename2;
  GoogleString content = "Lorem ipsum dolor sit amet";
  // We need to sleep a bit between accessing files so that the
  // difference shows up in in atimes which are measured in seconds.
  unsigned int sleep_us = 1500000;

  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path1.c_str(), content, &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path2.c_str(), content, &handler_));

  int64 atime1, atime2;
  CheckRead(full_path1, content);
  timer()->SleepUs(sleep_us);
  CheckRead(full_path2, content);
  ASSERT_TRUE(file_system()->Atime(full_path1, &atime1, &handler_));
  ASSERT_TRUE(file_system()->Atime(full_path2, &atime2, &handler_));
  EXPECT_LT(atime1, atime2);

  CheckRead(full_path2, content);
  timer()->SleepUs(sleep_us);
  CheckRead(full_path1, content);
  ASSERT_TRUE(file_system()->Atime(full_path1, &atime1, &handler_));
  ASSERT_TRUE(file_system()->Atime(full_path2, &atime2, &handler_));
  EXPECT_LT(atime2, atime1);
}

void FileSystemTest::TestMtime() {
  GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir");
  DeleteRecursively(dir_name);
  GoogleString filename1 = "file-in-dir.txt";
  GoogleString filename2 = "another-file-in-dir.txt";
  GoogleString full_path1 = dir_name + "/" + filename1;
  GoogleString full_path2 = dir_name + "/" + filename2;
  GoogleString content = "Lorem ipsum dolor sit amet";
  // We need to sleep a bit between accessing files so that the
  // difference shows up in in atimes which are measured in seconds.
  unsigned int sleep_us = 1500000;

  // Setup directory to play in.
  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));

  // Write two files with pause between
  ASSERT_TRUE(file_system()->WriteFile(full_path1.c_str(), content, &handler_));
  timer()->SleepUs(sleep_us);
  ASSERT_TRUE(file_system()->WriteFile(full_path2.c_str(), content, &handler_));

  int64 mtime1_orig, mtime2_orig;
  // Check that File1 was created before File2.
  ASSERT_TRUE(file_system()->Mtime(full_path1, &mtime1_orig, &handler_));
  ASSERT_TRUE(file_system()->Mtime(full_path2, &mtime2_orig, &handler_));
  EXPECT_LT(mtime1_orig, mtime2_orig);

  int64 mtime1_read, mtime2_read;
  // And that even if you read from File1 later, the C-time is still preserved.
  timer()->SleepUs(sleep_us);
  CheckRead(full_path1, content);
  ASSERT_TRUE(file_system()->Mtime(full_path1, &mtime1_read, &handler_));
  ASSERT_TRUE(file_system()->Mtime(full_path2, &mtime2_read, &handler_));
  EXPECT_EQ(mtime1_orig, mtime1_read);
  EXPECT_EQ(mtime2_orig, mtime2_read);

  int64 mtime1_recreate, mtime2_recreate;
  // But if we delete File1 and re-create it, the C-time is updated.
  timer()->SleepUs(sleep_us);
  ASSERT_TRUE(file_system()->RemoveFile(full_path1.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path1.c_str(), content, &handler_));
  ASSERT_TRUE(file_system()->Mtime(full_path1, &mtime1_recreate, &handler_));
  ASSERT_TRUE(file_system()->Mtime(full_path2, &mtime2_recreate, &handler_));
  EXPECT_LT(mtime1_orig, mtime1_recreate);
  EXPECT_EQ(mtime2_orig, mtime2_recreate);

  EXPECT_GT(mtime1_recreate, mtime2_recreate);
}

void FileSystemTest::TestDirInfo() {
  GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir");
  DeleteRecursively(dir_name);
  GoogleString dir_name2 = dir_name + "/make_dir2";
  GoogleString dir_name3 = dir_name + "/make_dir3/";
  GoogleString filename1 = "another-file-in-dir.txt";
  GoogleString filename2 = "file-in-dir.txt";
  GoogleString full_path1 = dir_name2 + "/" + filename1;
  GoogleString full_path2 = dir_name2 + "/" + filename2;
  GoogleString content1 = "12345";
  // Make content2 longer than the default block size of 4096 to make sure disk
  // based file systems are calculating the on-disk size, not just apparent size
  // of file. stdio and apr file systems should report 8192 as the size of
  // content2.
  GoogleString content2(4097, 'a');
  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  ASSERT_TRUE(file_system()->MakeDir(dir_name2.c_str(), &handler_));
  ASSERT_TRUE(file_system()->MakeDir(dir_name3.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path1.c_str(),
                                       content1, &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path2.c_str(),
                                       content2, &handler_));

  int64 size;
  EXPECT_TRUE(file_system()->Size(full_path1, &size, &handler_));
  EXPECT_EQ(FileSize(content1), size);
  EXPECT_TRUE(file_system()->Size(full_path2, &size, &handler_));
  EXPECT_EQ(FileSize(content2), size);

  FileSystem::DirInfo dir_info;
  FileSystem::DirInfo dir_info2;
  CountingProgressNotifier notifier1;
  file_system()->GetDirInfoWithProgress(
      dir_name2, &dir_info2, &notifier1, &handler_);
  EXPECT_EQ(FileSize(content1) + FileSize(content2), dir_info2.size_bytes);
  EXPECT_EQ(2, dir_info2.inode_count);
  EXPECT_EQ(static_cast<size_t>(2), dir_info2.files.size());
  // GetDirInfoWithProgress doesn't commit to a specific number of Notify()
  // calls, but it should be at least one.
  EXPECT_LE(1, notifier1.get_count());
  // dir_info.files is not guaranteed to be in any particular order, and in fact
  // come back in different order for mem and apr filesystems, so sort it so
  // that the comparison is consistent.
  std::sort(dir_info2.files.begin(), dir_info2.files.end(), CompareByName());
  EXPECT_STREQ(full_path1, dir_info2.files[0].name);
  EXPECT_STREQ(full_path2, dir_info2.files[1].name);
  EXPECT_EQ(static_cast<size_t>(0), dir_info2.empty_dirs.size());

  CountingProgressNotifier notifier2;
  file_system()->GetDirInfoWithProgress(
      dir_name, &dir_info, &notifier2, &handler_);
  int dir_size = DefaultDirSize();
  EXPECT_EQ(dir_size * 2 + FileSize(content1) + FileSize(content2),
            dir_info.size_bytes);
  EXPECT_EQ(4, dir_info.inode_count);
  // Running on this larger directory structure we should have at least one more
  // Notify() call than on the smaller one.
  EXPECT_LT(notifier1.get_count(), notifier2.get_count());
  std::sort(dir_info.files.begin(), dir_info.files.end(), CompareByName());
  EXPECT_STREQ(full_path1, dir_info.files[0].name);
  EXPECT_STREQ(full_path2, dir_info.files[1].name);
  EXPECT_EQ(static_cast<size_t>(1), dir_info.empty_dirs.size());
}

void FileSystemTest::TestLock() {
  GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir");
  DeleteRecursively(dir_name);
  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  GoogleString lock_name = dir_name + "/lock";
  // Acquire the lock
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_true());
  // Can't re-acquire the lock
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_false());
  // Release the lock
  EXPECT_TRUE(file_system()->Unlock(lock_name, &handler_));
  // Do it all again to make sure the release worked.
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_true());
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_false());
  EXPECT_TRUE(file_system()->Unlock(lock_name, &handler_));
}

// Test lock timeout; assumes the file system has at least 1-second creation
// granularity.
void FileSystemTest::TestLockTimeout() {
  GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir");
  DeleteRecursively(dir_name);
  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  GoogleString lock_name = dir_name + "/lock";
  // Acquire the lock
  EXPECT_TRUE(file_system()->TryLockWithTimeout(lock_name, Timer::kSecondMs,
                                                timer(), &handler_).is_true());
  // Immediate re-acquire should fail.  Steal time deliberately long so we don't
  // steal by mistake (since we're running in non-mock time).
  EXPECT_TRUE(file_system()->TryLockWithTimeout(lock_name, Timer::kMinuteMs,
                                                timer(), &handler_).is_false());
  // Wait just over 1 second so that we're past the timout.
  // Now we should seize lock.
  timer()->SleepMs(Timer::kSecondMs + 1);
  EXPECT_TRUE(file_system()->TryLockWithTimeout(lock_name, Timer::kSecondMs,
                                                timer(), &handler_).is_true());
  // Lock should still be held.
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_false());
  EXPECT_TRUE(file_system()->Unlock(lock_name, &handler_));
  // The result of this second unlock is unknown, but it ought not to crash.
  file_system()->Unlock(lock_name, &handler_);
  // Lock should now be unambiguously unlocked.
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_true());
}

void FileSystemTest::TestLockBumping() {
  const GoogleString dir_name = StrCat(test_tmpdir(), "/make_dir");
  DeleteRecursively(dir_name);
  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  const GoogleString lock_name = dir_name + "/lock";

  // No one holds the lock, so it can't be bumped.
  EXPECT_FALSE(file_system()->BumpLockTimeout(lock_name, &handler_));

  // Take the lock.
  EXPECT_TRUE(file_system()->TryLockWithTimeout(lock_name, Timer::kSecondMs * 3,
                                                timer(), &handler_).is_true());

  // Sleep 2s.  We still hold the lock.
  timer()->SleepMs(Timer::kSecondMs * 2);

  // Bump the lock.
  EXPECT_TRUE(file_system()->BumpLockTimeout(lock_name, &handler_));

  // Try to take the lock again.  This should fail even if bumping didn't work
  // because the original lock was only 2s old anyway.
  EXPECT_FALSE(file_system()->TryLockWithTimeout(
      lock_name, Timer::kSecondMs * 3, timer(), &handler_).is_true());

  // Bump the lock again to deflake this test on the CI vm.
  EXPECT_TRUE(file_system()->BumpLockTimeout(lock_name, &handler_));

  // Sleep 2s.  We still hold the lock, because we bumped it.
  timer()->SleepMs(Timer::kSecondMs * 2);

  // Try to take the lock again.  If bumping didn't work, then the lock would
  // have expired 1s ago and we could have taken it here.
  EXPECT_FALSE(file_system()->TryLockWithTimeout(
      lock_name, Timer::kSecondMs * 3, timer(), &handler_).is_true());

  // Sleep 2s.  The lock is now fully timed out.
  timer()->SleepMs(Timer::kSecondMs * 2);

  // With the lock timed out it's available for the taking.
  EXPECT_TRUE(file_system()->TryLockWithTimeout(lock_name, Timer::kSecondMs * 3,
                                                timer(), &handler_).is_true());
}

}  // namespace net_instaweb
