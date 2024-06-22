export module system.events;

export
{

    class EventArgs
    {
    public:
        EventArgs()
        {
        }
    };

    class ResizeEventArgs : public EventArgs
    {
    public:
        typedef EventArgs base;
        ResizeEventArgs(int width, int height) : Width(width), Height(height)
        {
        }

        // The new width of the window
        int Width;
        // The new height of the window.
        int Height;
    };

    class UpdateEventArgs : public EventArgs
    {
    public:
        typedef EventArgs base;
        UpdateEventArgs(double fDeltaTime, double fTotalTime) : ElapsedTime(fDeltaTime), TotalTime(fTotalTime)
        {
        }

        double ElapsedTime;
        double TotalTime;
    };

    class RenderEventArgs : public EventArgs
    {
    public:
        typedef EventArgs base;
        RenderEventArgs(double fDeltaTime, double fTotalTime) : ElapsedTime(fDeltaTime), TotalTime(fTotalTime)
        {
        }

        double ElapsedTime;
        double TotalTime;
    };

    class UserEventArgs : public EventArgs
    {
    public:
        typedef EventArgs base;
        UserEventArgs(int code, void* data1, void* data2) : Code(code), Data1(data1), Data2(data2)
        {
        }

        int Code;
        void* Data1;
        void* Data2;
    };
}
