struct Cache<T>
where T:Fn(u32)->u32
{
    query:T,
    value:Option<u32>,
}

impl<T>Cache<T>
where T:Fn(u32)->u32
{
    fn new(query:T)->Cache<T>
    {
        Cache{
            query,
            value:None,
        }
    }

    fn value(&mut self,arg:u32)->u32
    {
        match self.value {
            Some(v)=>v,
            None=>{
                let v=(self.query)(arg);
                self.value=Some(v);
                v
            }
        }
    }
}
fn main()
{

}