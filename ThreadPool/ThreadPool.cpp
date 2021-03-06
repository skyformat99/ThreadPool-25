#include <mutex>
#include <thread>
#include <iostream>
#include "MyThread.h"
#include "ThreadPool.h"

// Construct the ThreadPool
ThreadPool::ThreadPool(int threadNum):m_iThreadCnt(threadNum),
	m_pIdelContainer(nullptr),
	m_bStopPool(false),
	m_bForceStop(false),
	m_strErrorText(""),
	m_bDisposed(false)
{
	// Create idel container
	CreateIdelContainer();
}

// Detructe the ThreadPool
ThreadPool::~ThreadPool()
{
	if (!GetDisposed())
	{
		SetDisposed(true);

		Stop(true);
	}
}

// Create idel container
void ThreadPool::CreateIdelContainer()
{
	SetIdelTable(new IdelThreadContainer(this,GetThreadCnt()));
}

// Destory idel container
void ThreadPool::DestoryIdelContainer()
{
	if (GetIdelTable())
	{
		delete GetIdelTable();

		SetIdelTable(nullptr);
	}
}

// Run the pool
void ThreadPool::Run()
{
	while (1)
	{
		if (GetStopPool())
		{
			if (GetForceStop())
			{
				// Destory idel container
				DestoryIdelContainer();

				std::cout << "Force to exit the thread pool" << std::endl;

				break;
			}
			else
			{
				{
					std::lock_guard<std::mutex> Locker(m_WorkLock);

					if (!m_WorkContainer.IsEmpty())
					{
						std::cout << "Wait for work container empty" << std::endl;

						continue;
					}
				}

				// Destory idel container
				DestoryIdelContainer();

				std::cout << "Not force to exit the pool" << std::endl;

				break;
			}
		}

		// Get a task
		TaskEntry task;
		if (!GetOneTask(task))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			continue;
		}

		std::cout << "Get a task to run: " <<std::to_string(task.iTaskId)<< std::endl;

		// Get an idel thread 
		MyThread* pThread = GetAnIdelThread();
		if (pThread==nullptr)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			continue;
		}

		std::cout << "Get an idel thread to have the task : "<<std::to_string(pThread->GetId())<< std::endl;

		std::cout << "Idel container size :" << GetIdelTable()->GetSize() << std::endl;

		// configure the thread
		if (!ConfigureThread(pThread, task, true))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			continue;
		}

		// Add the thread to work container
		AddToWorkContainer(pThread);

		std::cout << "Add the idel thread to work container" << std::endl;

		std::cout << "work container size :"<< m_WorkContainer.GetSize() << std::endl;

		// Start thread
		StartThread(pThread);

		std::cout << "Start task working:"<< std::to_string(pThread->GetId()) << std::endl;
	}
}

// Start thread
void ThreadPool::StartThread(MyThread* pThread)
{
	pThread->Start();
}

// configure the thread
bool ThreadPool::ConfigureThread(MyThread* pThread, TaskEntry& task, bool bDetached)
{
	if (pThread==nullptr)
	{
		SetErrorText("Thread is invalid !");

		return false;
	}

	pThread->SetTask(task);

	pThread->SetDetachState(bDetached);

	return true;
}

// Start pool
void ThreadPool::Start()
{
	m_MonitorThread = std::thread(&ThreadPool::Run, this);
}

// Stop pool
int ThreadPool::Stop(bool bForce)
{
	SetStopPool(true);

	SetForceStop(bForce);

	if (m_MonitorThread.joinable())
	{
		m_MonitorThread.join();
	}

	return 0;
}

// Get a task
bool ThreadPool::GetOneTask(TaskEntry& task)
{
	if (m_TaskContainer.IsEmpty())
	{
		return false;
	}

	std::lock_guard<std::mutex> Locker(m_TaskLock);

	if (!m_TaskContainer.GetOneTask(task))
	{
		return false;
	}

	return true;
}

// Add Task to pool
int ThreadPool::AddTask(TaskEntry& task)
{
	std::lock_guard<std::mutex> Locker(m_TaskLock);

	if (m_TaskContainer.AddTask(task) == 1)
	{
		SetErrorText("Task container is full now !");

		return 1;
	}

	return 0;
}

// Get an idel thread
MyThread* ThreadPool::GetAnIdelThread()
{
	if (GetIdelTable()==nullptr)
	{
		return nullptr;
	}

	if (GetIdelTable()->IsEmpty())
	{
		return nullptr;
	}

	std::lock_guard<std::mutex> Locker(m_IdelLock);

	return GetIdelTable()->GetTopThread();
}

// Add to work container
bool ThreadPool::AddToWorkContainer(MyThread* pThread)
{
	if (pThread==nullptr)
	{
		return false;
	}

	std::lock_guard<std::mutex> Locker(m_WorkLock);

	m_WorkContainer.Add(pThread);

	return true;
}

// Remove from the work container
bool ThreadPool::RemoveFromWorkContainer(MyThread* pThread)
{
	if (pThread == nullptr)
	{
		return false;
	}

	std::lock_guard<std::mutex> Locker(m_WorkLock);

	m_WorkContainer.Remove(pThread);

	return true;
}

// Add to idel container
bool ThreadPool::AddToIdelContainer(MyThread* pThread)
{
	if (pThread == nullptr)
	{
		return false;
	}

	std::lock_guard<std::mutex> Locker(m_IdelLock);

	GetIdelTable()->AddThread(pThread);

	return true;
}

// Transfer thread to container
bool ThreadPool::Transfer(MyThread* pThread)
{
	std::cout << "Enter the final transfer job" << std::endl;

	// Remove from the work list
	if (!RemoveFromWorkContainer(pThread))
	{
		SetErrorText("Failed to remove the thread from working container!");

		return false;
	}

	std::cout << "Finish the removing from work container:"<< std::to_string(pThread->GetId()) << std::endl;

	std::cout << "Final work container size :" << m_WorkContainer.GetSize() << std::endl;

	// Add to the idel table
	if (!AddToIdelContainer(pThread))
	{
		SetErrorText("Failed to add the thread to idel container!");

		return false;
	}

	std::cout << "Re-add to the idel container" << std::endl;

	std::cout << "Final idel container size :" << GetIdelTable()->GetSize() << std::endl;

	return true;
}

// Get error message
std::string ThreadPool::GetErrorMsg()
{
	return GetErrorText();
}
