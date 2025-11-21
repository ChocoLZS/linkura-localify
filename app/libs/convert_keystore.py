#!/usr/bin/env python3
"""
Convert JKS keystore to BKS format using Java and BouncyCastle
"""

import subprocess
import os
import sys

# Java code to convert JKS to BKS
java_code = """
import java.io.*;
import java.security.*;

public class ConvertKeystore {
    public static void main(String[] args) throws Exception {
        if (args.length != 4) {
            System.err.println("Usage: ConvertKeystore <input.jks> <output.bks> <password> <alias>");
            System.exit(1);
        }

        String inputFile = args[0];
        String outputFile = args[1];
        String password = args[2];
        String alias = args[3];

        // Register BouncyCastle provider
        Security.addProvider(new org.bouncycastle.jce.provider.BouncyCastleProvider());

        // Load JKS keystore
        KeyStore jks = KeyStore.getInstance("JKS");
        try (FileInputStream fis = new FileInputStream(inputFile)) {
            jks.load(fis, password.toCharArray());
        }

        // Create BKS keystore
        KeyStore bks = KeyStore.getInstance("BKS", "BC");
        bks.load(null, null);

        // Copy key entry
        Key key = jks.getKey(alias, password.toCharArray());
        java.security.cert.Certificate[] chain = jks.getCertificateChain(alias);

        if (key == null) {
            System.err.println("Key not found: " + alias);
            System.exit(1);
        }

        bks.setKeyEntry(alias, key, password.toCharArray(), chain);

        // Save BKS keystore
        try (FileOutputStream fos = new FileOutputStream(outputFile)) {
            bks.store(fos, password.toCharArray());
        }

        System.out.println("Successfully converted " + inputFile + " to " + outputFile);
    }
}
"""

def main():
    # Assume lspatch.jar has been extracted to current directory (.)
    # Run from extracted jar directory (tmp), like rm_duplicate.py
    input_keystore = "assets/keystore"
    output_keystore = "assets/keystore.bks"

    # Write Java source
    with open("ConvertKeystore.java", "w") as f:
        f.write(java_code)

    # Compile Java code with current directory as classpath (BouncyCastle is already extracted)
    print("Compiling converter...")
    result = subprocess.run([
        "javac", "-cp", ".", "ConvertKeystore.java"
    ], capture_output=True, text=True)

    if result.returncode != 0:
        print("Compilation failed:")
        print(result.stderr)
        return 1

    # Run converter
    print("Converting keystore...")
    result = subprocess.run([
        "java", "-cp", ".",
        "ConvertKeystore",
        input_keystore,      # input JKS
        output_keystore,     # output BKS
        "123456",            # password
        "key0"               # alias
    ], capture_output=True, text=True)

    if result.returncode != 0:
        print("Conversion failed:")
        print(result.stderr)
        return 1

    print(result.stdout)

    # Cleanup
    os.remove("ConvertKeystore.java")
    os.remove("ConvertKeystore.class")

    return 0

if __name__ == "__main__":
    sys.exit(main())
