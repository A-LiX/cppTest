#include <iostream>
#include <vector>

void quick_sort(std::vector<int> &arr, int left, int right)
{
    if (left >= right)
        return;
    int flag = arr[left];
    int i=left;
    int j=right;
    for(;j>i;j--)
    {
        if(arr[j]<flag){
            arr[i] = arr[j];
            for(;i<j;i++)
            {
                if(arr[i]>flag){
                    arr[j]=arr[i];
                    break;
                }
            }
            
        }
    }
    if(j==i){
        arr[i] = flag;
    }
    
    quick_sort(arr, left, i -1 );
    quick_sort(arr, i + 1, right);
}

int main()
{
    std::vector<int> arr = {32, 47, 94, 54, 35, 55, 19};

    quick_sort(arr, 0, 6);

    std::cout << "After sort: ";
    for (auto v : arr)
        std::cout << v << " ";
    std::cout << std::endl;

    return 0;
}