package com.demo.nativeservice;

/** Demo native service interface */
interface IDemoService {
    int add(int a, int b);
    String getName();
    void setVerbose(boolean enable);
}
