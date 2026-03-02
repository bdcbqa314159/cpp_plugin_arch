// C# class library targeting .NET Framework 2.0+ (no modern features).
// Compiled by csc.exe into ManagedMath.dll, then referenced via #using
// from the C++/CLI bridge.

using System;

namespace ManagedMath
{
    public class Calculator
    {
        // --- Scalar (blittable) ---

        public double Add(double a, double b)
        {
            return a + b;
        }

        public double Multiply(double a, double b)
        {
            return a * b;
        }

        public double Sqrt(double a)
        {
            return Math.Sqrt(a);
        }

        // --- Batch (array, for pinning demo) ---

        /// <summary>
        /// Multiplies every element of <paramref name="values"/> by
        /// <paramref name="factor"/> in place.
        /// </summary>
        public void BatchMultiply(double[] values, double factor)
        {
            for (int i = 0; i < values.Length; i++)
            {
                values[i] *= factor;
            }
        }

        // --- String (marshaling demo) ---

        /// <summary>
        /// Returns a formatted string like "result_name = 42.00".
        /// </summary>
        public string FormatResult(string name, double value)
        {
            return name + " = " + value.ToString("F2");
        }
    }
}
