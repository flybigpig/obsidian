// IMyAidlInterface.aidl
package kim.hsl.aidl_demo;

import kim.hsl.aidl_demo.Student;

// Declare any non-default types here with import statements

interface IMyAidlInterface {
    /**
     * Demonstrates some basic types that you can use as parameters
     * and return values in AIDL.
     */
    void basicTypes(int anInt, long aLong, boolean aBoolean, float aFloat,
            double aDouble, String aString);

    /**
     * in 写入, out 输出, inout 写入和输出
     */
    void addStudent(inout Student student);

    /**
     * 获取 Student 集合
     */
    List<Student> getStudents();
}
