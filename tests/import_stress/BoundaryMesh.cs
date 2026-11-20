using System;
using System.Globalization;
using System.IO;
using System.Text;

// Build with the Windows .NET Framework csc.exe /optimize+ /target:exe.
// BoundaryMesh.exe [--plan] OUT_DIR TARGET_BYTES [TARGET_BYTES ...]
internal static class BoundaryMesh
{
    const double Radius = 20.0;
    const string Tail = "\n  </AppendedData>\n</VTKFile>\n";

    struct Vec
    {
        public double X, Y, Z;
        public Vec(double x, double y, double z) { X = x; Y = y; Z = z; }
        public static Vec operator +(Vec a, Vec b) { return new Vec(a.X + b.X, a.Y + b.Y, a.Z + b.Z); }
        public static Vec operator -(Vec a, Vec b) { return new Vec(a.X - b.X, a.Y - b.Y, a.Z - b.Z); }
        public static Vec operator *(Vec a, double k) { return new Vec(a.X * k, a.Y * k, a.Z * k); }
        public Vec Cross(Vec b) { return new Vec(Y * b.Z - Z * b.Y, Z * b.X - X * b.Z, X * b.Y - Y * b.X); }
        public double Length() { return Math.Sqrt(X * X + Y * Y + Z * Z); }
    }

    sealed class Sphere
    {
        public readonly int PointCount, FaceCount;
        readonly int columns, rings, extra, cap;
        readonly double[] cosPhi, sinPhi, cosTheta, sinTheta;

        public Sphere(int points)
        {
            PointCount = points;
            FaceCount = checked(2 * (points - 2));
            columns = (int)Math.Sqrt(2.0 * (points - 2));
            rings = (points - 2) / columns;
            extra = (points - 2) % columns;
            cap = 1 + columns * rings;
            cosPhi = new double[columns]; sinPhi = new double[columns];
            cosTheta = new double[rings]; sinTheta = new double[rings];
            for (int j = 0; j < columns; ++j)
            {
                double phi = (2.0 * Math.PI / columns) * j;
                cosPhi[j] = Math.Cos(phi); sinPhi[j] = Math.Sin(phi);
            }
            for (int i = 0; i < rings; ++i)
            {
                double theta = (Math.PI / (rings + 1)) * (i + 1);
                cosTheta[i] = Math.Cos(theta); sinTheta[i] = Math.Sin(theta);
            }
        }

        // Cartesian unit directions. Any extra vertices subdivide the north fan.
        public Vec UnitPoint(int id)
        {
            if (id == 0) return new Vec(0, 0, 1);
            if (id == cap) return new Vec(0, 0, -1);
            if (id > cap)
            {
                int j = id - cap - 1;
                Vec centroid = UnitPoint(0) + UnitPoint(j + 1) + UnitPoint(j + 2);
                return centroid * (1.0 / centroid.Length());
            }
            int ring = (id - 1) / columns, jcol = (id - 1) % columns;
            return new Vec(sinTheta[ring] * cosPhi[jcol], sinTheta[ring] * sinPhi[jcol], cosTheta[ring]);
        }

        public void Faces(Action<int, int, int> visit)
        {
            for (int j = 0; j < columns; ++j)
            {
                int a = 1 + j, b = 1 + (j + 1) % columns;
                if (j < extra)
                {
                    int c = cap + 1 + j;
                    visit(0, a, c); visit(a, b, c); visit(b, 0, c);
                }
                else visit(0, a, b);
            }
            for (int i = 0; i + 1 < rings; ++i)
            {
                int start = 1 + i * columns;
                for (int j = 0; j < columns; ++j)
                {
                    int a = start + j, b = start + (j + 1) % columns;
                    visit(a, a + columns, b);
                    visit(b, a + columns, b + columns);
                }
            }
            for (int j = 0; j < columns; ++j)
                visit(cap, cap - columns + (j + 1) % columns, cap - columns + j);
        }

        public Vec TwiceAreaVector(int a, int b, int c)
        {
            Vec u = UnitPoint(a);
            return (UnitPoint(b) - u).Cross(UnitPoint(c) - u) * (Radius * Radius);
        }
    }

    // Each array is flushed to disk in fixed-size blocks. VTK raw data uses little endian.
    sealed class BlockWriter
    {
        readonly Stream stream;
        readonly long[] words = new long[131072];
        readonly byte[] bytes = new byte[131072 * 8];
        int count;
        public BlockWriter(Stream stream) { this.stream = stream; }
        public void Word(long value)
        {
            words[count++] = value;
            if (count == words.Length) Flush();
        }
        public void Number(double value) { Word(BitConverter.DoubleToInt64Bits(value)); }
        public void Vector(Vec v) { Number(v.X); Number(v.Y); Number(v.Z); }
        public void Flush()
        {
            Buffer.BlockCopy(words, 0, bytes, 0, count * 8);
            stream.Write(bytes, 0, count * 8);
            count = 0;
        }
    }

    static long[] ArraySizes(long p)
    {
        long f = 2 * (p - 2);
        return new long[] { 24 * p, 24 * f, 8 * f, 24 * p, 24 * f, 8 * f };
    }

    static string Data(string type, string name, int components, long off)
    {
        return String.Format(CultureInfo.InvariantCulture,
            "        <DataArray type=\"{0}\" Name=\"{1}\" NumberOfComponents=\"{2}\" format=\"appended\" offset=\"{3}\"/>\n",
            type, name, components, off);
    }

    static string Header(long p, out long size)
    {
        long[] sizes = ArraySizes(p), off = new long[sizes.Length];
        long position = 0;
        for (int i = 0; i < sizes.Length; ++i)
        {
            off[i] = position;
            position += 8 + sizes[i];
        }
        string header = "<?xml version=\"1.0\"?>\n<VTKFile type=\"PolyData\" version=\"1.0\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n  <PolyData>\n" +
            String.Format(CultureInfo.InvariantCulture, "    <Piece NumberOfPoints=\"{0}\" NumberOfPolys=\"{1}\" NumberOfVerts=\"0\" NumberOfLines=\"0\" NumberOfStrips=\"0\">\n", p, 2 * (p - 2)) +
            "      <PointData Normals=\"AnalyticNormal\">\n" + Data("Float64", "AnalyticNormal", 3, off[3]) + "      </PointData>\n" +
            "      <CellData Normals=\"FaceNormal\" Scalars=\"FaceArea\">\n" + Data("Float64", "FaceNormal", 3, off[4]) + Data("Float64", "FaceArea", 1, off[5]) + "      </CellData>\n" +
            "      <Points>\n" + Data("Float64", "Points", 3, off[0]) + "      </Points>\n" +
            "      <Polys>\n" + Data("Int64", "connectivity", 1, off[1]) + Data("Int64", "offsets", 1, off[2]) + "      </Polys>\n" +
            "    </Piece>\n  </PolyData>\n  <AppendedData encoding=\"raw\">\n_";
        size = Encoding.ASCII.GetByteCount(header) + position + Encoding.ASCII.GetByteCount(Tail);
        return header;
    }

    static int PointsFor(long target)
    {
        if (target < 10000 || target > 10000000000L) throw new ArgumentOutOfRangeException("target", "use 10,000 to 10,000,000,000 bytes");
        int p = (int)Math.Max(16, target / 176);
        long size;
        Header(p, out size);
        while (size < target) Header(++p, out size);
        for (;;)
        {
            Header(p - 1, out size);
            if (size < target) return p;
            --p;
        }
    }

    static void Text(Stream stream, string value)
    {
        byte[] array = Encoding.ASCII.GetBytes(value);
        stream.Write(array, 0, array.Length);
    }

    static void Generate(string path, int points, long expected)
    {
        var s = new Sphere(points);
        var sizes = ArraySizes(points);
        string temp = path + "." + Guid.NewGuid().ToString("N") + ".partial";
        if (File.Exists(path)) throw new IOException("Output exists: " + path);
        try
        {
            using (var stream = new FileStream(temp, FileMode.CreateNew, FileAccess.Write, FileShare.None, 1048576))
            {
                long size;
                Text(stream, Header(points, out size));
                var w = new BlockWriter(stream);
                w.Word(sizes[0]);
                for (int i = 0; i < points; ++i) w.Vector(s.UnitPoint(i) * Radius);
                w.Word(sizes[1]);
                s.Faces(delegate(int a, int b, int c) { w.Word(a); w.Word(b); w.Word(c); });
                w.Word(sizes[2]);
                for (int i = 1; i <= s.FaceCount; ++i) w.Word(3L * i);
                w.Word(sizes[3]);
                for (int i = 0; i < points; ++i) w.Vector(s.UnitPoint(i));
                w.Word(sizes[4]);
                s.Faces(delegate(int a, int b, int c) {
                    Vec n = s.TwiceAreaVector(a, b, c);
                    w.Vector(n * (1.0 / n.Length()));
                });
                w.Word(sizes[5]);
                s.Faces(delegate(int a, int b, int c) { w.Number(0.5 * s.TwiceAreaVector(a, b, c).Length()); });
                w.Flush();
                Text(stream, Tail);
                if (stream.Position != expected) throw new InvalidDataException("Size disagrees with the mesh header");
            }
            File.Move(temp, path);
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }

    static int Main(string[] args)
    {
        try
        {
            bool plan = args.Length > 0 && args[0] == "--plan";
            int index = plan ? 1 : 0;
            if (args.Length - index < 2) throw new ArgumentException("BoundaryMesh.exe [--plan] OUTPUT_DIR TARGET_BYTES [TARGET_BYTES ...]");
            if (!BitConverter.IsLittleEndian) throw new PlatformNotSupportedException("A little-endian runtime is required.");
            string dir = Path.GetFullPath(args[index++]);
            if (!plan) Directory.CreateDirectory(dir);
            for (; index < args.Length; ++index)
            {
                long target = Int64.Parse(args[index], CultureInfo.InvariantCulture);
                int p = PointsFor(target);
                long size; Header(p, out size);
                string label = target % 1000000000L == 0 ? (target / 1000000000L) + "GB" : target + "B";
                string name = "boundary-" + label + ".vtp";
                if (!plan) Generate(Path.Combine(dir, name), p, size);
                Console.WriteLine("{0} target={1} bytes={2} points={3} triangles={4}", name, target, size, p, 2L * (p - 2));
            }
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            return 1;
        }
    }
}
