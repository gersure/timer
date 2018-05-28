#ifndef SINGLE_H
#define SINGLE_H

template <typename T>
class Singleton
{
public:
    static T& Instance()
    {
        static T impl;
        return impl;
    }
protected:
    Singleton(void) {}
    virtual ~Singleton(void) { }

private:
    Singleton(const Singleton& rhs)  = delete;
    Singleton& operator = (const Singleton& rhs) = delete;
};

//#include <mutex>
//
//template <typename T>
//class Singleton
//{
//public:
//    static T& Instance()
//    {
//        if (m_pInstance == NULL)
//        {
//            std::lock_guard<std::mutex> lck(m_mutex);
//            if (m_pInstance == NULL)
//            {
//                m_pInstance = new T();
//            }
//            return *m_pInstance;
//        }
//        return *m_pInstance;
//    }
//
//
//protected:
//    Singleton(void) { auto buildCG __attribute__((unused))=&garbo_; }
//    virtual ~Singleton(void) { }
//
//    class CGarbo
//    {
//        public:
//        ~CGarbo()
//        {
//            if(Singleton::m_pInstance){
//                delete Singleton::m_pInstance;
//                Singleton::m_pInstance = nullptr;
//            }
//        }
//    };
//
//private:
//    Singleton(const Singleton& rhs)  = delete;
//    Singleton& operator = (const Singleton& rhs) = delete;
//
//    static CGarbo garbo_;
//    static std::mutex m_mutex;
//    static T* volatile m_pInstance;
//};
//
//template <typename T>
//T* volatile Singleton<T>::m_pInstance = NULL;
//template <typename T>
//std::mutex Singleton<T>::m_mutex;
//template <typename T>
//typename Singleton<T>::CGarbo Singleton<T>::garbo_;

#endif // SINGLE_H
