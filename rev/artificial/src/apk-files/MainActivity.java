package com.example.android;

import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

public class MainActivity extends AppCompatActivity {
    private Graph graph;

    public static List<Pair> parsePairs(String input) {
        String[] numbers = input.split(",");
        List<Pair> pairs = new ArrayList<>();
        for (int i = 0; i < numbers.length; i += 2) {
            int first = Integer.parseInt(numbers[i]);
            int second = Integer.parseInt(numbers[i + 1]);
            pairs.add(new Pair(first, second));
        }
        return pairs;
    }

    private boolean check(List<Pair> pairs) {
        int total = 0;
        Set<Pair> visited = new HashSet<>();
        for (Pair p : pairs) {
            if (!this.graph.hasEdge(p.first, p.second))
                return false;
            if (p.first >= p.second)
                return false;
            if (!visited.add(p))
                return false;
            total += this.graph.getEdge(p.first, p.second);
        }

        return pairs.size() == 999 && total == 18726440 && graph.checkConnectivity(pairs);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        EditText editText = findViewById(R.id.editTextInput);
        Button button = findViewById(R.id.buttonConvert);
        TextView textView = findViewById(R.id.textViewOutput);

        editText.setMovementMethod(new ScrollingMovementMethod());

        this.graph = new Graph(1000);
        this.graph.load();

        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String text = editText.getText().toString();
                List<Pair> pairs = null;
                try {
                    pairs = parsePairs(text);
                } catch (Exception e) {
                    textView.setText("Invalid Format! Expected: 9,2,4,...");
                }
                if (check(pairs)) {
                    byte[] encrypted = new byte[] { (byte) 231, (byte) 83, (byte) 49, (byte) 90, (byte) 239, (byte) 168,
                            (byte) 241, (byte) 52, (byte) 114, (byte) 216, (byte) 104, (byte) 253, (byte) 2, (byte) 108,
                            (byte) 224, (byte) 47, (byte) 229, (byte) 56, (byte) 176, (byte) 103, (byte) 44, (byte) 124,
                            (byte) 5, (byte) 81, (byte) 74, (byte) 210, (byte) 233, (byte) 37, (byte) 219, (byte) 182,
                            (byte) 58, (byte) 192 };
                    byte[] flag = null;
                    try {
                        flag = Win.decrypt(Win.serializePairs(pairs), encrypted);
                        textView.setText(new String(flag));
                    } catch (Exception e) {
                        e.printStackTrace();
                        textView.setText(
                                "An error occurred. Please contact an admin if you believe you have the right key.");
                    }
                } else {
                    textView.setText("Wrong key!");
                }
            }
        });
    }
}