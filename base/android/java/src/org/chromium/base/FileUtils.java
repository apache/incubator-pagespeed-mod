// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;
import java.util.Locale;

/**
 * Helper methods for dealing with Files.
 */
public class FileUtils {
    private static final String TAG = "FileUtils";

    /**
     * Delete the given File and (if it's a directory) everything within it.
     * @param currentFile The file or directory to delete. Does not need to exist.
     * @return Whether currentFile does not exist afterwards.
     */
    public static boolean recursivelyDeleteFile(File currentFile) {
        if (!currentFile.exists()) {
            return true;
        }
        if (currentFile.isDirectory()) {
            File[] files = currentFile.listFiles();
            if (files != null) {
                for (File file : files) {
                    recursivelyDeleteFile(file);
                }
            }
        }

        boolean ret = currentFile.delete();
        if (!ret) {
            Log.e(TAG, "Failed to delete: %s", currentFile);
        }
        return ret;
    }

    /**
     * Delete the given files or directories by calling {@link #recursivelyDeleteFile(File)}. This
     * supports deletion of content URIs.
     * @param filePaths The file paths or content URIs to delete.
     */
    public static void batchDeleteFiles(List<String> filePaths) {
        for (String filePath : filePaths) {
            if (ContentUriUtils.isContentUri(filePath)) {
                ContentUriUtils.delete(filePath);
            } else {
                File file = new File(filePath);
                if (file.exists()) recursivelyDeleteFile(file);
            }
        }
    }

    /**
     * Extracts an asset from the app's APK to a file.
     * @param context
     * @param assetName Name of the asset to extract.
     * @param outFile File to extract the asset to.
     * @return true on success.
     */
    public static boolean extractAsset(Context context, String assetName, File outFile) {
        try (InputStream inputStream = context.getAssets().open(assetName)) {
            copyStreamToFile(inputStream, outFile);
            return true;
        } catch (IOException e) {
            return false;
        }
    }

    /**
     * Performs a simple copy of inputStream to outputStream.
     */
    public static void copyStream(InputStream inputStream, OutputStream outputStream)
            throws IOException {
        byte[] buffer = new byte[8192];
        int amountRead;
        while ((amountRead = inputStream.read(buffer)) != -1) {
            outputStream.write(buffer, 0, amountRead);
        }
    }

    /**
     * Atomically copies the data from an input stream into an output file.
     * @param is Input file stream to read data from.
     * @param outFile Output file path.
     * @throws IOException in case of I/O error.
     */
    public static void copyStreamToFile(InputStream is, File outFile) throws IOException {
        File tmpOutputFile = new File(outFile.getPath() + ".tmp");
        try (OutputStream os = new FileOutputStream(tmpOutputFile)) {
            Log.i(TAG, "Writing to %s", outFile);
            copyStream(is, os);
        }
        if (!tmpOutputFile.renameTo(outFile)) {
            throw new IOException();
        }
    }

    /**
     * Reads inputStream into a byte array.
     */
    @NonNull
    public static byte[] readStream(InputStream inputStream) throws IOException {
        ByteArrayOutputStream data = new ByteArrayOutputStream();
        FileUtils.copyStream(inputStream, data);
        return data.toByteArray();
    }

    /**
     * Returns a URI that points at the file.
     * @param file File to get a URI for.
     * @return URI that points at that file, either as a content:// URI or a file:// URI.
     */
    public static Uri getUriForFile(File file) {
        // TODO(crbug/709584): Uncomment this when http://crbug.com/709584 has been fixed.
        // assert !ThreadUtils.runningOnUiThread();
        Uri uri = null;

        try {
            // Try to obtain a content:// URI, which is preferred to a file:// URI so that
            // receiving apps don't attempt to determine the file's mime type (which often fails).
            uri = ContentUriUtils.getContentUriFromFile(file);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Could not create content uri: " + e);
        }

        if (uri == null) uri = Uri.fromFile(file);

        return uri;
    }

    /**
     * Returns the file extension, or an empty string if none.
     * @param file Name of the file, with or without the full path.
     * @return empty string if no extension, extension otherwise.
     */
    public static String getExtension(String file) {
        int index = file.lastIndexOf('.');
        if (index == -1) return "";
        return file.substring(index + 1).toLowerCase(Locale.US);
    }

    /** Queries and decodes bitmap from content provider. */
    @Nullable
    public static Bitmap queryBitmapFromContentProvider(Context context, Uri uri) {
        try (ParcelFileDescriptor parcelFileDescriptor =
                        context.getContentResolver().openFileDescriptor(uri, "r")) {
            if (parcelFileDescriptor == null) {
                Log.w(TAG, "Null ParcelFileDescriptor from uri " + uri);
                return null;
            }
            FileDescriptor fileDescriptor = parcelFileDescriptor.getFileDescriptor();
            if (fileDescriptor == null) {
                Log.w(TAG, "Null FileDescriptor from uri " + uri);
                return null;
            }
            Bitmap bitmap = BitmapFactory.decodeFileDescriptor(fileDescriptor);
            if (bitmap == null) {
                Log.w(TAG, "Failed to decode image from uri " + uri);
                return null;
            }
            return bitmap;
        } catch (IOException e) {
            Log.w(TAG, "IO exception when reading uri " + uri);
        }
        return null;
    }
}
