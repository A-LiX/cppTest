#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>
#include <unordered_map>

struct Order
{
    int order_id;
    std::string name;
    double price;
    int quantity;
    int timestamp;
    std::string side;
};

struct Account
{
    std::string name;
    int position;
    double income;
    double expense;
};

struct MatchingPriceInfo
{
    double price;
    double amount;
    int bid_volume;
    int offer_volume;
};

std::multimap<double, Order> bid_map;
std::multimap<double, Order> offer_map;
std::unordered_map<std::string, Account> accounts_map;
std::map<double, MatchingPriceInfo> matching_price_map;

int cur_cross_bid_volume = 0;
int cur_cross_offer_volume = 0;
double best_matching_price = 0;

void calc_best_matching_price(Order &order)
{

    if (order.side == "BUY")
    {
        bid_map.emplace(order.price, order);
    }
    else if (order.side == "SELL")
    {
        offer_map.emplace(order.price, order);
    }

    if (!bid_map.empty() && !offer_map.empty() && bid_map.rbegin()->first >= offer_map.begin()->first)
    {

        double upper_bound_price = bid_map.rbegin()->first;
        double lower_bound_price = offer_map.begin()->first;

        if (best_matching_price == 0)
        {
            std::multimap<double, Order>::iterator offer_order_itr = offer_map.begin();
            std::multimap<double, Order>::reverse_iterator bid_order_itr = bid_map.rbegin();
            double start_price = offer_order_itr->first;
            double end_price = bid_order_itr->first;

            while (bid_order_itr != bid_map.rend() && bid_order_itr->first >= start_price)
            {
                if (matching_price_map.find(bid_order_itr->first) == matching_price_map.end())
                {
                    matching_price_map.emplace(bid_order_itr->first, MatchingPriceInfo{bid_order_itr->first, 0, 0, 0});
                }
                matching_price_map[bid_order_itr->first].bid_volume += bid_order_itr->second.quantity;

                auto itr = offer_map.begin();
                while (itr != offer_map.end() && itr->first <= bid_order_itr->first)
                {
                    matching_price_map[bid_order_itr->first].offer_volume += itr->second.quantity;
                    itr++;
                }

                bid_order_itr++;
            }

            while (offer_order_itr != offer_map.end() && offer_order_itr->first <= end_price)
            {
                if (matching_price_map.find(offer_order_itr->first) == matching_price_map.end())
                {
                    matching_price_map.emplace(offer_order_itr->first, MatchingPriceInfo{offer_order_itr->first, 0, 0, 0});
                }
                matching_price_map[offer_order_itr->first].offer_volume += offer_order_itr->second.quantity;
                auto itr = bid_map.rbegin();
                while (itr != bid_map.rend() && itr->first >= offer_order_itr->first)
                {
                    matching_price_map[offer_order_itr->first].bid_volume += itr->second.quantity;
                    itr++;
                }
                offer_order_itr++;
            }

            double cur_max_matching_amount = 0;
            for (auto &matching_info : matching_price_map)
            {
                int matching_volume = std::min(matching_info.second.bid_volume, matching_info.second.offer_volume);
                double matching_price = matching_info.first;
                matching_info.second.amount = matching_volume * matching_price;

                if (matching_info.second.amount >= cur_max_matching_amount)
                {
                    best_matching_price = matching_price;
                    cur_max_matching_amount = matching_info.second.amount;
                }
            }
        }
        else if (order.price <= upper_bound_price && order.price >= lower_bound_price)
        {

            if (matching_price_map.find(order.price) == matching_price_map.end())
            {
                matching_price_map.emplace(order.price, MatchingPriceInfo{order.price, 0, 0, 0});

                auto itr = matching_price_map.find(order.price);
                auto prev_matching_itr = --itr;
                ++itr;
                auto next_matching_itr = ++itr;
                --itr;

                if (prev_matching_itr != matching_price_map.begin())
                {
                    itr->second.offer_volume = prev_matching_itr->second.offer_volume;
                }

                if (next_matching_itr != matching_price_map.end())
                {
                    itr->second.bid_volume = next_matching_itr->second.bid_volume;
                }

                if (order.side == "BUY")
                {
                    matching_price_map[order.price].bid_volume += order.quantity;
                }
                else if (order.side == "SELL")
                {
                    matching_price_map[order.price].offer_volume += order.quantity;
                }

                //calculate the amount for the new price
                matching_price_map[order.price].amount = order.price * std::min(matching_price_map[order.price].bid_volume, matching_price_map[order.price].offer_volume);
            }
            else
            {
                if (order.side == "BUY")
                {
                    matching_price_map[order.price].bid_volume += order.quantity;
                }
                else if (order.side == "SELL")
                {
                    matching_price_map[order.price].offer_volume += order.quantity;
                }
                
                matching_price_map[order.price].amount = order.price * std::min(matching_price_map[order.price].bid_volume, matching_price_map[order.price].offer_volume);
            }

            if (matching_price_map[order.price].amount > matching_price_map[best_matching_price].amount)
            {
                best_matching_price = order.price;
            }
            else if (matching_price_map[order.price].amount == matching_price_map[best_matching_price].amount && order.price > best_matching_price)
            {
                best_matching_price = order.price;
            }
            return;
        }
    }
}

void auction_matching(double best_matching_price)
{
    std::multimap<double, Order>::iterator offer_order_itr = offer_map.begin();
    std::multimap<double, Order>::iterator bid_order_itr = --bid_map.end();

    std::vector<std::multimap<double, Order>::iterator> offer_orders_vec;
    std::vector<std::multimap<double, Order>::iterator> bid_orders_vec;

    int available_bid_volume = matching_price_map[best_matching_price].bid_volume;
    int available_offer_volume = matching_price_map[best_matching_price].offer_volume;

    while (offer_order_itr != offer_map.end() && offer_order_itr->first <= best_matching_price)
    {
        offer_orders_vec.push_back(offer_order_itr);
        offer_order_itr++;
    }

    while (bid_order_itr != bid_map.begin() && bid_order_itr->first >= best_matching_price)
    {
        bid_orders_vec.push_back(bid_order_itr);
        bid_order_itr--;
    }

    if (bid_order_itr == bid_map.begin() && bid_order_itr->first >= best_matching_price)
    {
        available_bid_volume += bid_order_itr->second.quantity;
        bid_orders_vec.push_back(bid_order_itr);
    }

    if (available_bid_volume == 0 || available_offer_volume == 0)
    {
        return;
    }

    int matching_volume = std::min(available_bid_volume, available_offer_volume);

    // sort the orders based on price, quantity and timestamp
    sort(offer_orders_vec.begin(), offer_orders_vec.end(),
         [](const auto &order1, const auto &order2)
         { 
             if(order1->second.price != order2->second.price)
                 return order1->second.price > order2->second.price;
             else if(order1->second. quantity!= order2->second.quantity)
                 return order1->second.quantity > order2->second.quantity;
             else
                 return order1->second.timestamp < order2->second.timestamp; });

    sort(bid_orders_vec.begin(), bid_orders_vec.end(),
         [](const auto &order1, const auto &order2)
         { 
             if(order1->second.price != order2->second.price)
                 return order1->second.price > order2->second.price;
             else if(order1->second.quantity != order2->second.quantity)
                 return order1->second.quantity > order2->second.quantity;
             else
                 return order1->second.timestamp < order2->second.timestamp; });

    int available_offer_volume_count = matching_volume;
    int available_bid_volume_count = matching_volume;
    for (auto &of_itr : offer_orders_vec)
    {
        if (of_itr->second.quantity < available_offer_volume_count)
        {
            accounts_map[of_itr->second.name].position -= of_itr->second.quantity;
            accounts_map[of_itr->second.name].income += of_itr->second.quantity * best_matching_price;
            available_offer_volume_count -= of_itr->second.quantity;
            offer_map.erase(of_itr);
        }
        else
        {
            accounts_map[of_itr->second.name].position -= available_offer_volume_count;
            accounts_map[of_itr->second.name].income += available_offer_volume_count * best_matching_price;

            if (of_itr->second.quantity == available_offer_volume_count)
            {
                offer_map.erase(of_itr);
            }

            available_offer_volume_count = 0;
            break;
        }
    }

    for (auto &bd_itr : bid_orders_vec)
    {
        if (bd_itr->second.quantity < available_bid_volume_count)
        {
            accounts_map[bd_itr->second.name].position += bd_itr->second.quantity;
            accounts_map[bd_itr->second.name].expense += bd_itr->second.quantity * best_matching_price;
            available_bid_volume_count -= bd_itr->second.quantity;
            bid_map.erase(bd_itr);
        }
        else
        {
            accounts_map[bd_itr->second.name].position += available_bid_volume_count;
            accounts_map[bd_itr->second.name].expense += available_bid_volume_count * best_matching_price;

            if (bd_itr->second.quantity == available_bid_volume_count)
            {
                bid_map.erase(bd_itr);
            }

            available_bid_volume_count = 0;
            break;
        }
    }
}

int main()
{
    // Read orders from csv file
    std::ifstream file("orders.csv");
    if (!file.is_open())
    {
        std::cerr << "failed to open orders.csv" << std::endl;
        return 0;
    }

    std::string order_str;
    while (std::getline(file, order_str))
    {
        std::stringstream ss(order_str);
        Order order;

        // Parse data
        ss >> order.order_id;
        ss.ignore(1);
        std::getline(ss, order.name, ',');
        ss >> order.price;
        ss.ignore(1);
        ss >> order.quantity;
        ss.ignore(1);
        ss >> order.timestamp;
        ss.ignore(1);
        std::getline(ss, order.side, '\r');

        // calculate best matching price continuously
        calc_best_matching_price(order);

        // create account for everyone
        if (accounts_map.find(order.name) == accounts_map.end())
        {
            accounts_map.emplace(order.name, Account{order.name});
        }
    }

    auction_matching(best_matching_price);

    // Output

    std::cout << std::left << std::setw(10) << "Name"
              << std::setw(10) << "Position"
              << std::setw(10) << "Amount"
              << std::setw(16) << "Auction Price"
              << std::setw(10) << "Profits" << std::endl;
    std::cout << std::string(54, '-') << std::endl;

    for (const auto &account : accounts_map)
    {
        std::cout << std::left << std::setw(10) << account.first;
        if (account.second.position > 0)
            std::cout << std::setw(10) << " L " << std::setw(10) << account.second.position;
        else
            std::cout << std::setw(10) << " S " << std::setw(10) << -account.second.position;
        std::cout << std::setw(16) << best_matching_price;
        std::cout << std::setw(10) << account.second.income - account.second.expense + account.second.position * best_matching_price << std::endl;
    }

    file.close();
    return 0;
}
