package com.example.android;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

public class Win {
    public static String serializePairs(List<Pair> pairs) {
        // Sort the pairs before serialization
        Collections.sort(pairs, new Comparator<Pair>() {
            @Override
            public int compare(Pair o1, Pair o2) {
                if (o1.first != o2.first) {
                    return Integer.compare(o1.first, o2.first);
                } else {
                    return Integer.compare(o1.second, o2.second);
                }
            }
        });

        StringBuilder concatenated = new StringBuilder();
        for (Pair pair : pairs) {
            concatenated.append(pair.first);
            concatenated.append(pair.second);
        }
        return concatenated.toString();
    }

    public static byte[] decrypt(String serializedKey, byte[] encryptedData) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        byte[] hashedKey = digest.digest(serializedKey.getBytes(StandardCharsets.UTF_8));
        hashedKey = Arrays.copyOf(hashedKey, 16);

        SecretKeySpec secretKey = new SecretKeySpec(hashedKey, "AES");
        Cipher cipher = Cipher.getInstance("AES");
        cipher.init(Cipher.DECRYPT_MODE, secretKey);
        return cipher.doFinal(encryptedData);
    }

}
