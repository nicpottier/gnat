require 'fileutils'

module Filter
  def self.process(site, payload)
    print("hi")    
    site.collections['releases'].docs.each do |x|
      print("hi")
      f = x.destination("")
      print(f.split("/"))
      new_destination = f.split("/")[0..-2].join("/") + "/#{x.data['slug']}.json"
      FileUtils.cp f, new_destination
    end
  end
end

Jekyll::Hooks.register :site, :post_write do |site, payload|
  Filter.process(site, payload)
end